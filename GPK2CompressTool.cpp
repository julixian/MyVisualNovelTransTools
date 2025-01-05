#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct Header {
    char magic[4];
    uint32_t uncomplen;
    uint32_t version;
};

// LZSS decompression function (unchanged)
static uint32_t lzss_decompress(uint8_t* uncompr, uint32_t uncomprlen, const uint8_t* compr, uint32_t comprlen) {
    uint32_t act_uncomprlen = 0;
    uint32_t curbyte = 0;
    uint32_t nCurWindowByte = 0xfee;
    uint32_t win_size = 4096;
    std::vector<uint8_t> win(win_size, 0);
    uint16_t flag = 0;

    while (curbyte < comprlen) {
        flag >>= 1;
        if (!(flag & 0x0100)) {
            if (curbyte >= comprlen) break;
            flag = compr[curbyte++] | 0xff00;
        }
        if (flag & 1) {
            if (act_uncomprlen >= uncomprlen) break;
            uint8_t data = compr[curbyte++];
            uncompr[act_uncomprlen++] = data;
            win[nCurWindowByte++] = data;
            nCurWindowByte &= win_size - 1;
        }
        else {
            if (curbyte + 1 >= comprlen) break;
            uint32_t win_offset = compr[curbyte++];
            uint32_t copy_bytes = compr[curbyte++];
            win_offset |= (copy_bytes & 0xf0) << 4;
            copy_bytes = (copy_bytes & 0x0f) + 3;
            for (uint32_t i = 0; i < copy_bytes; i++) {
                if (act_uncomprlen >= uncomprlen) break;
                uint8_t data = win[(win_offset + i) & (win_size - 1)];
                uncompr[act_uncomprlen++] = data;
                win[nCurWindowByte++] = data;
                nCurWindowByte &= win_size - 1;
            }
        }
    }
    return act_uncomprlen;
}

// LZSS pseudo-compression function
std::vector<uint8_t> lzss_pseudo_compress(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    const size_t input_size = input.size();
    size_t input_pos = 0;

    while (input_pos < input_size) {
        uint8_t flag = 0xFF;  // All bits set to 1, indicating direct copy for next 8 operations
        output.push_back(flag);

        for (int i = 0; i < 8 && input_pos < input_size; ++i) {
            output.push_back(input[input_pos++]);
        }
    }

    return output;
}

void compress_file(const fs::path& input_path, const fs::path& output_path) {
    std::ifstream input_file(input_path, std::ios::binary);
    if (!input_file) {
        std::cerr << "Cannot open input file: " << input_path << std::endl;
        return;
    }

    std::vector<uint8_t> input_data((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
    input_file.close();

    std::vector<uint8_t> compressed_data = lzss_pseudo_compress(input_data);

    // Encrypt compressed data
    for (auto& byte : compressed_data) {
        byte = ~byte;
    }

    Header header = { 'G', 'L', 'P', 'K', static_cast<uint32_t>(input_data.size()), 1 };

    std::ofstream output_file(output_path, std::ios::binary);
    if (!output_file) {
        std::cerr << "Cannot create output file: " << output_path << std::endl;
        return;
    }

    output_file.write(reinterpret_cast<char*>(&header), sizeof(header));
    output_file.write(reinterpret_cast<char*>(compressed_data.data()), compressed_data.size());

    std::cout << "Compressed file saved as: " << output_path << std::endl;
}

void decompress_file(const fs::path& input_path, const fs::path& output_path) {
    std::ifstream file(input_path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file: " << input_path << std::endl;
        return;
    }

    Header header;
    file.read(reinterpret_cast<char*>(&header), sizeof(Header));

    if (std::strncmp(header.magic, "GLPK", 3) != 0) {
        std::cerr << "Invalid file header: " << input_path << std::endl;
        return;
    }

    file.seekg(0, std::ios::end);
    uint32_t filesize = file.tellg();
    file.seekg(sizeof(Header), std::ios::beg);

    std::vector<uint8_t> compressedData(filesize - sizeof(Header));
    file.read(reinterpret_cast<char*>(compressedData.data()), compressedData.size());

    // Decrypt
    for (auto& byte : compressedData) {
        byte = ~byte;
    }

    std::vector<uint8_t> uncompressedData(header.uncomplen);
    uint32_t actualUncompLen = lzss_decompress(uncompressedData.data(), header.uncomplen,
        compressedData.data(), compressedData.size());

    std::ofstream outFile(output_path, std::ios::binary);
    if (!outFile) {
        std::cerr << "Cannot create output file: " << output_path << std::endl;
        return;
    }

    outFile.write(reinterpret_cast<char*>(uncompressedData.data()), actualUncompLen);
    std::cout << "Decompressed file saved as: " << output_path << std::endl;
}

void process_directory(const fs::path& input_dir, const fs::path& output_dir, bool is_compressing) {
    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            fs::path input_path = entry.path();
            fs::path output_path = output_dir / input_path.filename();

            if (is_compressing) {
                if (input_path.extension() != ".scb") {
                    output_path.replace_extension(".scb");
                    compress_file(input_path, output_path);
                }
            }
            else {
                if (input_path.extension() == ".scb") {
                    output_path.replace_extension();
                    decompress_file(input_path, output_path);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.01.01" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <e|d> <input_directory> <output_directory>" << std::endl;
        return 1;
    }

    bool is_compressing = (std::string(argv[1]) == "e");
    fs::path input_dir(argv[2]);
    fs::path output_dir(argv[3]);

    if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) {
        std::cerr << "Input directory does not exist or is not a directory." << std::endl;
        return 1;
    }

    if (!fs::exists(output_dir)) {
        fs::create_directories(output_dir);
    }

    process_directory(input_dir, output_dir, is_compressing);

    std::cout << "All files processed." << std::endl;
    return 0;
}
