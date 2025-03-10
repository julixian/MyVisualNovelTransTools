#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <algorithm>

namespace fs = std::filesystem;

std::vector<uint8_t> compress(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    for (size_t i = 0; i < input.size(); i += 8) {
        output.push_back(0xFF);
        for (size_t j = 0; j < 8 && i + j < input.size(); ++j) {
            output.push_back(input[i + j]);
        }
    }
    return output;
}

class LzssDecompressor {
public:
    LzssDecompressor(int frameSize = 0x1000, uint8_t frameFill = 0, int frameInitPos = 0xFEE)
        : m_frameSize(frameSize), m_frameFill(frameFill), m_frameInitPos(frameInitPos) {
    }

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        std::vector<uint8_t> frame(m_frameSize, m_frameFill);
        int framePos = m_frameInitPos;
        int frameMask = m_frameSize - 1;

        size_t inputPos = 0;
        while (inputPos < input.size()) {
            uint8_t ctrl = input[inputPos++];
            for (int bit = 1; bit != 0x100 && inputPos < input.size(); bit <<= 1) {
                if (ctrl & bit) {
                    uint8_t b = input[inputPos++];
                    frame[framePos++ & frameMask] = b;
                    output.push_back(b);
                }
                else {
                    if (inputPos + 1 >= input.size()) break;
                    uint8_t lo = input[inputPos++];
                    uint8_t hi = input[inputPos++];
                    int offset = ((hi & 0xf0) << 4) | lo;
                    int count = 3 + (hi & 0xF);

                    for (int i = 0; i < count; ++i) {
                        uint8_t v = frame[offset++ & frameMask];
                        frame[framePos++ & frameMask] = v;
                        output.push_back(v);
                    }
                }
            }
        }

        return output;
    }

private:
    int m_frameSize;
    uint8_t m_frameFill;
    int m_frameInitPos;
};

struct FileEntry {
    std::string filename;
    uint32_t offset;
    uint32_t size;
    std::string fullPath;
    std::vector<uint8_t> finalData;
};

uint32_t BigEndianToHost(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x000000FF) << 24);
}

uint32_t HostToBigEndian(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x000000FF) << 24);
}

std::string DecryptFileName(const std::vector<uint8_t>& encrypted_name, uint8_t name_length) {
    std::vector<uint8_t> decrypted_name = encrypted_name;
    uint8_t key = name_length + 1;

    for (int i = 0; i < name_length; ++i) {
        decrypted_name[i] -= key--;
        if (decrypted_name[i] < 0x20 || decrypted_name[i] >= 0xFD) {
            return ""; 
        }
    }

    return std::string(decrypted_name.begin(), decrypted_name.begin() + name_length);
}

std::vector<uint8_t> EncryptFileName(const std::string& filename) {
    std::vector<uint8_t> encrypted;
    uint8_t name_length = static_cast<uint8_t>(filename.length());
    uint8_t key = name_length + 1;

    for (char c : filename) {
        encrypted.push_back(static_cast<uint8_t>(c + key));
        key--;
    }

    return encrypted;
}

bool ExtractFiles(const std::string& dat_path, const std::string& output_dir, const std::vector<std::string>& extensions) {
    std::ifstream dat_file(dat_path, std::ios::binary);
    if (!dat_file) {
        std::cerr << "无法打开文件: " << dat_path << std::endl;
        return false;
    }

    uint32_t file_count;
    dat_file.read(reinterpret_cast<char*>(&file_count), sizeof(file_count));

    if (file_count == 0 || file_count > 10000) { 
        std::cerr << "Invalid file count: " << file_count << std::endl;
        return false;
    }

    fs::create_directories(output_dir);

    std::vector<FileEntry> entries;
    uint32_t current_offset = 4; 

    for (uint32_t i = 0; i < file_count; ++i) {
        FileEntry entry;

        uint8_t name_length;
        dat_file.read(reinterpret_cast<char*>(&name_length), 1);
        current_offset += 1;

        std::vector<uint8_t> encrypted_name(name_length);
        dat_file.read(reinterpret_cast<char*>(encrypted_name.data()), name_length);
        current_offset += name_length;

        entry.filename = DecryptFileName(encrypted_name, name_length);
        if (entry.filename.empty()) {
            std::cerr << "Fail to analyze file name" << std::endl;
            return false;
        }

        uint32_t offset, size;
        dat_file.read(reinterpret_cast<char*>(&offset), 4);
        dat_file.read(reinterpret_cast<char*>(&size), 4);
        current_offset += 8;

        entry.offset = BigEndianToHost(offset);
        entry.size = BigEndianToHost(size);

        entries.push_back(entry);
    }

    for (const auto& entry : entries) {
        std::cout << "Extracting: " << entry.filename
            << " (Offset: " << entry.offset
            << ", Size: " << entry.size << ")" << std::endl;

        std::string output_path = output_dir + "/" + entry.filename;
        fs::create_directories(fs::path(output_path).parent_path());
        std::ofstream output_file(output_path, std::ios::binary);

        if (!output_file) {
            std::cerr << "Can not create file: " << output_path << std::endl;
            continue;
        }

        dat_file.seekg(entry.offset);

        std::vector<uint8_t> buffer(entry.size);
        dat_file.read((char*)buffer.data(), entry.size);

        auto it = std::find(extensions.begin(), extensions.end(), entry.filename.substr(entry.filename.find_last_of(".")));
        std::vector<uint8_t> finalData;
        if (it != extensions.end()) {
            LzssDecompressor decompressor;
            finalData = decompressor.decompress(buffer);
        }
        else {
            finalData = buffer;
        }

        output_file.write((char*)finalData.data(), finalData.size());
        output_file.close();
    }

    dat_file.close();
    return true;
}

bool CreatePackage(const std::string& input_dir, const std::string& output_path, const std::vector<std::string>& extensions) {
    std::vector<FileEntry> entries;
    uint32_t total_files = 0;


    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            FileEntry file_entry;
            file_entry.fullPath = entry.path().string();
            file_entry.filename = fs::relative(entry.path(), input_dir).string();

            if (file_entry.filename.length() > 255) {
                std::cerr << "file name is too long: " << file_entry.filename << std::endl;
                return false;
            }

            auto file_size = fs::file_size(entry.path());
            std::ifstream input(entry.path(), std::ios::binary);
            if (!input) {
                std::cerr << "Can not read file: " << file_entry.filename << std::endl;
                return false;
            }
            std::vector<uint8_t> buffer(file_size);
            input.read((char*)buffer.data(), file_size);
            auto it = std::find(extensions.begin(), extensions.end(), entry.path().extension().string());
            if (it != extensions.end()) {
                file_entry.finalData = compress(buffer);
            }
            else{
                file_entry.finalData = buffer;
            }
            file_entry.size = static_cast<uint32_t>(file_entry.finalData.size());
            entries.push_back(file_entry);
            total_files++;
        }
    }

    if (entries.empty()) {
        std::cerr << "No files in directory" << std::endl;
        return false;
    }

    uint32_t current_offset = 4;  

    for (auto& entry : entries) {
        current_offset += 1;  
        current_offset += entry.filename.length();  
        current_offset += 8; 
    }

    for (auto& entry : entries) {
        entry.offset = current_offset;
        current_offset += entry.size;
    }

    std::ofstream out_file(output_path, std::ios::binary);
    if (!out_file) {
        std::cerr << "无法创建输出文件: " << output_path << std::endl;
        return false;
    }

    uint32_t file_count = static_cast<uint32_t>(entries.size());
    out_file.write(reinterpret_cast<char*>(&file_count), 4);

    for (const auto& entry : entries) {
        uint8_t name_length = static_cast<uint8_t>(entry.filename.length());
        out_file.write(reinterpret_cast<char*>(&name_length), 1);

        std::vector<uint8_t> encrypted_name = EncryptFileName(entry.filename);
        out_file.write(reinterpret_cast<char*>(encrypted_name.data()), encrypted_name.size());

        uint32_t offset_be = HostToBigEndian(entry.offset);
        uint32_t size_be = HostToBigEndian(entry.size);
        out_file.write(reinterpret_cast<char*>(&offset_be), 4);
        out_file.write(reinterpret_cast<char*>(&size_be), 4);
    }

    for (const auto& entry : entries) {
        std::cout << "Packing: " << entry.filename
            << " (Offset: " << entry.offset
            << ", Size: " << entry.size << ")" << std::endl;

        out_file.write((char*)entry.finalData.data(), entry.size);
    }

    out_file.close();
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Made by julixian 2025.03.11" << std::endl;
        std::cout << "Usage: " << "\n"
            << "For extract: " << argv[0] << " -e <input_file> <output_dir> [--soc] [--scr] [--etc.]" << "\n"
            << "For pack: " << argv[0] << " -p <input_dir> <output_file> [--soc] [--scr] [--etc.]" << "\n"
            << "--etc.: " << "To set files with what extension will be decompressed/compressed when extracting/packing" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string path1 = argv[2];
    std::string path2 = argv[3];
    std::vector<std::string> extensions;
    
    for (size_t argOffset = 4; argOffset < argc; argOffset++) {
        std::string extension = argv[argOffset];
        extension = "." + extension.substr(2);
        extensions.push_back(extension);
    }

    if (mode == "-e") {
        if (ExtractFiles(path1, path2, extensions)) {
            std::cout << "Extracting successfully" << std::endl;
            return 0;
        }
        else {
            std::cerr << "Extracting failed" << std::endl;
            return 1;
        }
    }
    else if (mode == "-p") {
        if (CreatePackage(path1, path2, extensions)) {
            std::cout << "Packing successfully" << std::endl;
            return 0;
        }
        else {
            std::cerr << "Packing failed" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Not a valid mode" << std::endl;
        return 1;
    }
}
