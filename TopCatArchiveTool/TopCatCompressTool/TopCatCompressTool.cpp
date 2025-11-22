#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;

template<typename T>
T read(void* ptr)
{
    T value;
    memcpy(&value, ptr, sizeof(T));
    return value;
}

template<typename T>
void write(void* ptr, T value)
{
    memcpy(ptr, &value, sizeof(T));
}

std::string wide2Ascii(const std::wstring& wide, UINT CodePage = CP_UTF8);
std::wstring ascii2Wide(const std::string& ascii, UINT CodePage = CP_ACP);
std::string ascii2Ascii(const std::string& ascii, UINT src = CP_ACP, UINT dst = CP_UTF8);

std::string wide2Ascii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst) {
    return wide2Ascii(ascii2Wide(ascii, src), dst);
}


uint8_t rotateRight(uint8_t byte, int positions) {
    return (byte >> positions) | (byte << (8 - positions));
}
uint8_t rotateLeft(uint8_t byte, int positions) {
    return (byte << positions) | (byte >> (8 - positions));
}
void decrypt(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = rotateRight(data[i], 1);
    }
}
void encrypt(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = rotateLeft(data[i], 1);
    }
}

// ==========================================
// 压缩逻辑 (Compression)
// ==========================================

// 在窗口内查找最长匹配
struct MatchResult {
    int length;
    int offset;
};

MatchResult findLongestMatch(const std::vector<uint8_t>& data, int currentPos) {
    int maxOffset = 4095; // 12 bits
    int maxLen = 18;      // (0xF) + 3
    int minLen = 3;

    int startSearch = std::max(0, currentPos - maxOffset);
    int endSearch = currentPos;

    int bestLen = 0;
    int bestOffset = 0;

    // 简单线性搜索 (实际生产中可用 Hash Chain 或二叉树优化)
    for (int i = startSearch; i < endSearch; ++i) {
        int matchLen = 0;
        // 检查匹配长度
        while (matchLen < maxLen &&
            (currentPos + matchLen) < data.size() &&
            data[i + matchLen] == data[currentPos + matchLen]) {
            matchLen++;
        }

        if (matchLen > bestLen) {
            bestLen = matchLen;
            bestOffset = currentPos - i;
        }
    }

    if (bestLen < minLen) {
        return { 0, 0 };
    }
    return { bestLen, bestOffset };
}

std::vector<uint8_t> compressLz(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    int srcPos = 0;
    int srcSize = (int)input.size();

    while (srcPos < srcSize) {
        uint8_t flagByte = 0;
        std::vector<uint8_t> buffer; // 暂存这8个操作的数据

        // 处理 8 个标志位 (或者直到文件结束)
        for (int bit = 0; bit < 8; ++bit) {
            if (srcPos >= srcSize) {
                // 文件结束，剩下的位默认为 0 (但在解压时会检查 EOF，所以没关系)
                // 实际上解压逻辑是根据 flag 位来读流的，如果 flag 是 1 但流没了会报错，
                // 但这里我们只处理存在的字节。
                // 此时 flagByte 已经设置好，循环结束。
                break;
            }

            MatchResult match = findLongestMatch(input, srcPos);

            if (match.length >= 3) {
                // --- 压缩引用 (Bit = 0) ---
                // 不需要设置 flagByte 的位，因为默认是 0

                // 编码逻辑 (根据 C# 解压代码反推):
                // C#: offset = byte2 << 4 | byte1 >> 4
                // C#: count  = (byte1 & 0xF) + 3

                // 构造 Byte1:
                // 低4位 = length - 3
                // 高4位 = offset 的低4位
                uint8_t lenCode = (match.length - 3) & 0x0F;
                uint8_t offsetLow4 = match.offset & 0x0F;
                uint8_t byte1 = (offsetLow4 << 4) | lenCode;

                // 构造 Byte2:
                // offset 的高8位
                uint8_t byte2 = (match.offset >> 4) & 0xFF;

                buffer.push_back(byte1);
                buffer.push_back(byte2);

                srcPos += match.length;
            }
            else {
                // --- 原文 (Bit = 1) ---
                flagByte |= (1 << bit);
                buffer.push_back(input[srcPos]);
                srcPos++;
            }
        }

        // 1. 写入 Flag Byte
        output.push_back(flagByte);
        // 2. 写入暂存的数据
        output.insert(output.end(), buffer.begin(), buffer.end());
    }

    return output;
}


std::vector<uint8_t> decompressLz(const std::vector<uint8_t>& input, uint32_t unpackedSize) {
    std::vector<uint8_t> output;
    output.reserve(unpackedSize);

    int srcPos = 0;
    int bits = 2; // Initial state same as C# code

    while (output.size() < unpackedSize && srcPos < input.size()) {
        bits >>= 1;
        if (1 == bits) {
            if (srcPos >= input.size()) break;
            bits = input[srcPos++];
            bits |= 0x100;
        }

        if (0 == (bits & 1)) {
            // LZ Reference
            if (srcPos + 1 >= input.size()) break; // Safety check

            int countByte = input[srcPos++];
            int offsetByte = input[srcPos++];

            int offset = (offsetByte << 4) | (countByte >> 4);
            int count = (countByte & 0xF) + 3;

            // CopyOverlapped logic
            int dst = (int)output.size();
            // Limit count to remaining size? The C# code does: Math.Min(..., output.Length - dst)
            // Since we are pushing back, we just copy.

            for (int i = 0; i < count; ++i) {
                if (dst - offset + i < 0) {
                    // Should not happen with valid data
                    output.push_back(0);
                }
                else {
                    output.push_back(output[dst - offset + i]);
                }
            }
        }
        else {
            // Literal
            if (srcPos < input.size()) {
                output.push_back(input[srcPos++]);
            }
        }
    }
    return output;
}

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void decompress(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath, std::ios::binary);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    uint32_t unpackedSize;
    input.read(reinterpret_cast<char*>(&unpackedSize), 4);

    std::vector<uint8_t> compressedData(fs::file_size(inputPath) - 4);
    input.read(reinterpret_cast<char*>(compressedData.data()), compressedData.size());

    std::vector<uint8_t> decompressedData = decompressLz(compressedData, unpackedSize);
    decrypt(decompressedData);

    output.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());

    input.close();
    output.close();

    std::println("Decompression complete. Output saved to {}", wide2Ascii(outputPath));
}

void compress(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath, std::ios::binary);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    std::vector<uint8_t> inputData(fs::file_size(inputPath));
    input.read(reinterpret_cast<char*>(inputData.data()), inputData.size());
    encrypt(inputData);

    std::vector<uint8_t> compressedData = compressLz(inputData);

    uint32_t unpackedSize = (uint32_t)inputData.size();
    output.write(reinterpret_cast<const char*>(&unpackedSize), 4);
    output.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());

    input.close();
    output.close();

    std::println("Compression complete. Output saved to {}", wide2Ascii(outputPath));
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.22\n"
        "Usage: \n"
        "  decompress: {0} decompress <input_folder> <output_folder>\n"
        "  compress: {0} compress <input_folder> <output_folder>\n",
        wide2Ascii(programPath.filename()));
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        std::wstring mode = argv[1];
        if (mode == L"decompress") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            const fs::path inputFolder = argv[2];
            const fs::path outputFolder = argv[3];
            fs::create_directories(outputFolder);
            for (const auto& entry : fs::recursive_directory_iterator(inputFolder)) {
                if (entry.is_regular_file()) {
                    const fs::path inputPath = entry.path();
                    const fs::path outputPath = outputFolder / fs::relative(inputPath, inputFolder);
                    if (!fs::exists(outputPath.parent_path())) {
                        fs::create_directories(outputPath.parent_path());
                    }
                    decompress(inputPath, outputPath);
                }
            }
        }
        else if (mode == L"compress") {
            if(argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            const fs::path inputFolder = argv[2];
            const fs::path outputFolder = argv[3];
            fs::create_directories(outputFolder);
            for (const auto& entry : fs::recursive_directory_iterator(inputFolder)) {
                if (entry.is_regular_file()) {
                    const fs::path inputPath = entry.path();
                    const fs::path outputPath = outputFolder / fs::relative(inputPath, inputFolder);
                    if (!fs::exists(outputPath.parent_path())) {
                        fs::create_directories(outputPath.parent_path());
                    }
                    compress(inputPath, outputPath);
                }
            }
        }
        else {
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::print("Error: {0}", e.what());
        return 1;
    }

    return 0;
}
