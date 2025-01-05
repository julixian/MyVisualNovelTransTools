#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <memory>
#include <filesystem>
#include <bitset>
#include <cstring>

namespace fs = std::filesystem;

class MsbBitStream {
    std::ifstream& m_input;
    std::bitset<8> m_current_byte;
    int m_bit_position = 0;

public:
    MsbBitStream(std::ifstream& input) : m_input(input) {}

    int GetBits(int count) {
        int result = 0;
        for (int i = 0; i < count; ++i) {
            if (m_bit_position == 0) {
                char byte;
                m_input.read(&byte, 1);
                if (m_input.eof()) return -1;
                m_current_byte = std::bitset<8>(static_cast<unsigned char>(byte));
                m_bit_position = 8;
            }
            result = (result << 1) | m_current_byte[--m_bit_position];
        }
        return result;
    }
};

class HuffmanDecompressor {
    static const int TreeSize = 512;
    std::vector<uint16_t> lhs;
    std::vector<uint16_t> rhs;
    uint16_t m_token = 256;
    std::unique_ptr<MsbBitStream> m_input;

    uint16_t CreateTree() {
        int bit = m_input->GetBits(1);
        if (bit == -1) {
            throw std::runtime_error("Unexpected end of the Huffman-compressed stream.");
        }
        else if (bit != 0) {
            uint16_t v = m_token++;
            if (v >= TreeSize)
                throw std::runtime_error("Invalid Huffman-compressed stream.");
            lhs[v] = CreateTree();
            rhs[v] = CreateTree();
            return v;
        }
        else {
            return static_cast<uint16_t>(m_input->GetBits(8));
        }
    }

public:
    HuffmanDecompressor() : lhs(TreeSize), rhs(TreeSize) {}

    std::vector<uint8_t> Decompress(std::ifstream& input, size_t unpackedSize) {
        m_input = std::make_unique<MsbBitStream>(input);
        m_token = 256;
        uint16_t root = CreateTree();

        std::vector<uint8_t> output;
        output.reserve(unpackedSize);

        while (output.size() < unpackedSize) {
            uint16_t symbol = root;
            while (symbol >= 0x100) {
                int bit = m_input->GetBits(1);
                if (bit == -1) break;
                symbol = (bit != 0) ? rhs[symbol] : lhs[symbol];
            }
            if (symbol < 0x100) {
                output.push_back(static_cast<uint8_t>(symbol));
            }
        }

        return output;
    }
};

class HuffmanCompressor {
    static const int TreeSize = 512;
    std::vector<uint16_t> lhs;
    std::vector<uint16_t> rhs;
    int cache_bits = 0;
    int mm_bits = 0;

    void EncodeBit(int token, int token_width, std::ofstream& writer) {
        int mask = (1 << token_width) - 1;
        if (cache_bits < 24) {
            mm_bits <<= token_width;
            mm_bits |= (token & mask);
            cache_bits += token_width;
        }
        if (cache_bits >= 8) {
            while (cache_bits >= 8) {
                int shift = cache_bits - 8;
                uint8_t byte = (mm_bits >> shift) & 0xFF;
                cache_bits -= 8;
                int remain_mask = (1 << cache_bits) - 1;
                mm_bits &= remain_mask;
                writer.put(byte);
            }
        }
    }

    void WriteTree(std::ofstream& writer) {
        for (int i = 256; i < TreeSize; i++) {
            if (i == 511) {
                EncodeBit(0, 1, writer);
                EncodeBit(1, 8, writer);
            }
            else {
                EncodeBit(1, 1, writer);
            }
        }
        for (int j = 511; j > 255; j--) {
            EncodeBit(0, 1, writer);
            EncodeBit(rhs[j], 8, writer);
        }
    }

public:
    HuffmanCompressor() : lhs(TreeSize), rhs(TreeSize) {
        for (uint16_t i = 256; i < TreeSize; i++) {
            lhs[i] = i;
        }
        for (uint16_t i = 256, j = 0; i < TreeSize; i++, j++) {
            rhs[i] = j;
        }
        rhs[257] = 0;
    }

    std::vector<uint8_t> Compress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        std::ofstream writer("temp.bin", std::ios::binary);

        WriteTree(writer);

        int run_length = 9;
        for (uint8_t byte : input) {
            if (byte == 0) {
                EncodeBit(1, 1, writer);
            }
            else if (byte == 1) {
                for (int j = 0; j < 255; j++) {
                    EncodeBit(0, 1, writer);
                }
            }
            else {
                for (int k = 0; k < byte - 1; k++) {
                    if (run_length == 0) {
                        EncodeBit(0, 1, writer);
                    }
                    else {
                        run_length--;
                    }
                }
                EncodeBit(1, 1, writer);
            }
        }

        EncodeBit(255, 8, writer);
        EncodeBit(255, 8, writer);

        writer.close();

        std::ifstream reader("temp.bin", std::ios::binary);
        output.assign(std::istreambuf_iterator<char>(reader), std::istreambuf_iterator<char>());
        reader.close();

        fs::remove("temp.bin");

        return output;
    }
};

void decompressFile(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return;
    }

    // Read unpackedSize (first 4 bytes)
    uint32_t unpackedSize;
    input.read(reinterpret_cast<char*>(&unpackedSize), sizeof(unpackedSize));

    HuffmanDecompressor decompressor;
    std::vector<uint8_t> decompressedData = decompressor.Decompress(input, unpackedSize);

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return;
    }

    output.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
    std::cout << "Decompressed: " << inputPath << " -> " << outputPath << std::endl;
}

void compressFile(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return;
    }

    std::vector<uint8_t> inputData((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    HuffmanCompressor compressor;
    std::vector<uint8_t> compressedData = compressor.Compress(inputData);

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return;
    }

    // Write uncompressed size
    uint32_t uncompressedSize = inputData.size();
    output.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));

    // Write compressed data
    output.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());

    std::cout << "Compressed: " << inputPath << " -> " << outputPath << std::endl;
}

void processDirectory(const fs::path& inputDir, const fs::path& outputDir, bool compress) {
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            fs::path relativePath = fs::relative(entry.path(), inputDir);
            fs::path outputPath = outputDir / relativePath;

            fs::create_directories(outputPath.parent_path());

            if (compress) {
                compressFile(entry.path(), outputPath);
            }
            else {
                if (entry.path().extension() == ".scr") {
                    decompressFile(entry.path(), outputPath);
                }
                else {
                    fs::copy_file(entry.path(), outputPath, fs::copy_options::overwrite_existing);
                    std::cout << "Copied: " << entry.path() << " -> " << outputPath << std::endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.01.01" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <compress|decompress> <input_directory> <output_directory>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    fs::path inputDir = argv[2];
    fs::path outputDir = argv[3];

    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cerr << "Input directory does not exist or is not a directory." << std::endl;
        return 1;
    }

    fs::create_directories(outputDir);

    try {
        if (mode == "compress") {
            processDirectory(inputDir, outputDir, true);
            std::cout << "Compression complete." << std::endl;
        }
        else if (mode == "decompress") {
            processDirectory(inputDir, outputDir, false);
            std::cout << "Decompression complete." << std::endl;
        }
        else {
            std::cerr << "Invalid mode. Use 'compress' or 'decompress'." << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
