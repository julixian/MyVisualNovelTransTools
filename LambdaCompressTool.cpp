#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// MSB位流读取类
class MsbBitStream {
private:
    std::vector<uint8_t>& m_input;
    uint32_t m_bits;
    int m_bit_count;
    size_t& m_pos;

public:
    MsbBitStream(std::vector<uint8_t>& input, size_t& pos) : m_input(input), m_pos(pos), m_bits(0), m_bit_count(0) {}

    int GetBits(int count) {
        while (m_bit_count < count) {
            if (m_pos >= m_input.size())return -1;
            int byte = m_input[m_pos++];
            m_bits = (m_bits << 8) | byte;
            m_bit_count += 8;
        }
        int result = (m_bits >> (m_bit_count - count)) & ((1 << count) - 1);
        m_bit_count -= count;
        return result;
    }
};

// LZSS解压缩类
class LzssDecoder {
private:
    uint8_t frame[0x1000];
    int framePos;

public:
    LzssDecoder() : framePos(0xFEE) {
        memset(frame, 0, sizeof(frame));
    }

    size_t decode(std::vector<uint8_t>& input, size_t& pos, uint8_t* output, size_t outSize) {
        int bits = 2;
        size_t outPos = 0;

        while (outPos < outSize) {
            bits >>= 1;
            if (bits == 1) {
                if (pos >= input.size()) break;
                int byte = input[pos++];
                bits = byte | 0x100;
            }

            if (pos >= input.size()) break;
            int lo = input[pos++];

            if (bits & 1) {
                // 直接字节
                output[outPos++] = frame[framePos++ & 0xFFF] = lo;
            }
            else {
                // 压缩引用
                if (pos >= input.size()) break;
                int hi = input[pos++];
                int offset = ((hi & 0xF0) << 4) | lo;
                int count = std::min(3 + (hi & 0xF), (int)(outSize - outPos));

                while (count-- > 0) {
                    uint8_t v = frame[offset++ & 0xFFF];
                    output[outPos++] = frame[framePos++ & 0xFFF] = v;
                }
            }
        }
        return outPos;
    }
};

// Huffman解压缩类
class HuffmanDecoder {
private:
    static const int TREE_SIZE = 512;
    uint16_t lhs[TREE_SIZE];
    uint16_t rhs[TREE_SIZE];
    uint16_t m_token;
    MsbBitStream& m_input;

    uint16_t CreateTree() {
        int bit = m_input.GetBits(1);
        if (bit == -1) {
            throw std::runtime_error("Unexpected end of the Huffman-compressed stream.");
        }
        else if (bit != 0) {
            uint16_t v = m_token++;
            if (v >= TREE_SIZE)
                throw std::runtime_error("Invalid Huffman-compressed stream.");
            lhs[v] = CreateTree();
            rhs[v] = CreateTree();
            return v;
        }
        else {
            return m_input.GetBits(8);
        }
    }

public:
    HuffmanDecoder(MsbBitStream& input) : m_input(input), m_token(256) {
        memset(lhs, 0, sizeof(lhs));
        memset(rhs, 0, sizeof(rhs));
    }

    size_t decode(uint8_t* output, size_t outSize) {
        m_token = 256;
        uint16_t root = CreateTree();
        size_t outPos = 0;

        while (outPos < outSize) {
            uint16_t symbol = root;
            while (symbol >= 0x100) {
                int bit = m_input.GetBits(1);
                if (bit == -1) return outPos;
                symbol = (bit != 0) ? rhs[symbol] : lhs[symbol];
            }
            output[outPos++] = (uint8_t)symbol;
        }
        return outPos;
    }
};

bool DecompressFile(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream inFile(inputPath, std::ios::binary);
    if (!inFile) {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return false;
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inFile), {});
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return false;
    }

    for (size_t i = 0; i < buffer.size(); i++) {
        if (buffer[i] == 0x5f && buffer[i + 1] == 0x41 && buffer[i + 2] == 0x46 && buffer[i + 3] == 0x31) { //_AF1
            uint8_t header[10];
            memcpy(header, &buffer[i], 10);
            i += 10;
            uint8_t method = header[3];
            uint16_t chunk_size = *(uint16_t*)(header + 4);
            uint16_t final_size = *(uint16_t*)(header + 6);
            uint16_t unpacked_size = *(uint16_t*)(header + 8);

            // 确保输出缓冲区足够大
            std::vector<uint8_t> output(0x10000);
            if (output.size() < unpacked_size) {
                output.resize(unpacked_size);
            }

            size_t decompressed_size = 0;
            LzssDecoder decoder;
            decompressed_size = decoder.decode(buffer, i, output.data(), unpacked_size);
            output.resize(decompressed_size);
            outFile.write((char*)output.data(), output.size());
            //std::cout << "LZ  " << std::hex << i << std::endl;
            if (buffer[i] != 0x00)i--;
        }
        else if (buffer[i] == 0x5f && buffer[i + 1] == 0x41 && buffer[i + 2] == 0x46 && buffer[i + 3] == 0x32) {
            uint8_t header[10];
            memcpy(header, &buffer[i], 10);
            i += 10;
            uint8_t method = header[3];
            uint16_t chunk_size = *(uint16_t*)(header + 4);
            uint16_t final_size = *(uint16_t*)(header + 6);
            uint16_t unpacked_size = *(uint16_t*)(header + 8);

            // 确保输出缓冲区足够大
            std::vector<uint8_t> output(0x10000);
            if (output.size() < unpacked_size) {
                output.resize(unpacked_size);
            }

            size_t decompressed_size = 0;
            MsbBitStream bits(buffer, i);
            HuffmanDecoder decoder(bits);
            decompressed_size = decoder.decode(output.data(), unpacked_size);
            output.resize(decompressed_size);
            outFile.write((char*)output.data(), output.size());
            //std::cout << "Huffman  " << std::hex << i << std::endl;
            if (buffer[i] != 0x00)i--;
        }
        else {
            outFile.write((char*)&buffer[i], 1);
        }
    }

    return true;
}

void ProcessDirectory(const std::string& inputDir, const std::string& outputDir) {
    // 确保输出目录存在
    fs::create_directories(outputDir);

    // 遍历输入目录中的所有文件
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            std::string inputPath = entry.path().string();
            // 构建输出文件路径：保持相同的文件名
            std::string fileName = entry.path().filename().string();
            std::string outputPath = (fs::path(outputDir) / fileName).string();

            std::cout << "Processing: " << fileName << std::endl;

            if (DecompressFile(inputPath, outputPath)) {
                std::cout << "Successfully decompressed: " << fileName << std::endl;
            }
            else {
                std::cerr << "Failed to decompress: " << fileName << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Made by julixian 2025.02.10" << std::endl;
        std::cout << "Usage: " << argv[0] << " <input_directory> <output_directory>" << std::endl;
        return 1;
    }

    std::string inputDir = argv[1];
    std::string outputDir = argv[2];

    // 检查输入目录是否存在
    if (!fs::exists(inputDir)) {
        std::cerr << "Input directory does not exist: " << inputDir << std::endl;
        return 1;
    }

    // 检查输入路径是否是目录
    if (!fs::is_directory(inputDir)) {
        std::cerr << "Input path is not a directory: " << inputDir << std::endl;
        return 1;
    }

    try {
        ProcessDirectory(inputDir, outputDir);
        std::cout << "All files processed" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
