#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

class CmpReader {
private:
    std::ifstream& m_input;
    std::vector<uint8_t> m_output;
    int m_srcTotal;
    int m_srcCount = 0;
    int m_bits = 0;
    int m_cachedBits = 0;

    int getBits(int count) {
        while (m_cachedBits < count) {
            if (m_srcCount >= m_srcTotal) return -1;
            char b;
            if (!m_input.read(&b, 1)) return -1;
            m_bits = (m_bits << 8) | (uint8_t)b;
            m_cachedBits += 8;
            m_srcCount++;
        }
        int mask = (1 << count) - 1;
        m_cachedBits -= count;
        return (m_bits >> m_cachedBits) & mask;
    }

public:
    CmpReader(std::ifstream& file, int srcSize, int dstSize)
        : m_input(file), m_output(dstSize), m_srcTotal(srcSize) {}

    void unpack() {
        int dst = 0;
        std::vector<uint8_t> shift(0x800, 0x20);
        int edi = 0x7ef;

        while (dst < m_output.size()) {
            int bit = getBits(1);
            if (bit == -1) break;

            if (bit == 1) {
                int data = getBits(8);
                if (data == -1) break;
                m_output[dst++] = data;
                shift[edi++] = data;
                edi &= 0x7ff;
            }
            else {
                int offset = getBits(11);
                if (offset == -1) break;
                int count = getBits(4);
                if (count == -1) break;
                count += 2;

                for (int i = 0; i < count; ++i) {
                    uint8_t data = shift[(offset + i) & 0x7ff];
                    m_output[dst++] = data;
                    shift[edi++] = data;
                    edi &= 0x7ff;
                    if (dst == m_output.size()) return;
                }
            }
        }
    }

    const std::vector<uint8_t>& getData() const { return m_output; }
};

void decompressFile(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream file(inputPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << inputPath << std::endl;
        return;
    }

    // Read CMP1 signature
    char signature[5] = { 0 };
    file.read(signature, 4);
    if (std::string(signature) != "CMP1") {
        std::cerr << "Invalid signature for file: " << inputPath << std::endl;
        return;
    }

    // Read unpacked size
    uint32_t unpackedSize;
    file.read(reinterpret_cast<char*>(&unpackedSize), 4);

    // Calculate packed size
    file.seekg(0, std::ios::end);
    int packedSize = static_cast<int>(file.tellg()) - 12;
    file.seekg(12, std::ios::beg);

    // Decompress
    CmpReader reader(file, packedSize, unpackedSize);
    reader.unpack();

    // Write decompressed data
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return;
    }
    outFile.write(reinterpret_cast<const char*>(reader.getData().data()), unpackedSize);
    outFile.close();

    std::cout << "Decompressed: " << inputPath.filename() << " -> " << outputPath.filename() << std::endl;
}

class BitWriter {
private:
    std::ofstream& m_output;
    int m_bitBuffer = 0;
    int m_bitCount = 0;

public:
    BitWriter(std::ofstream& output) : m_output(output) {}

    void writeBits(int bits, int count) {
        m_bitBuffer = (m_bitBuffer << count) | bits;
        m_bitCount += count;
        while (m_bitCount >= 8) {
            m_bitCount -= 8;
            m_output.put(static_cast<char>((m_bitBuffer >> m_bitCount) & 0xFF));
        }
    }

    void flush() {
        if (m_bitCount > 0) {
            writeBits(0, 8 - m_bitCount);
        }
    }
};

void compressFile(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream inFile(inputPath, std::ios::binary);
    if (!inFile) {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inFile), {});
    inFile.close();

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return;
    }

    // Write CMP1 signature
    outFile.write("CMP1", 4);

    // Write original file size (4 bytes)
    uint32_t fileSize = static_cast<uint32_t>(buffer.size());
    outFile.write(reinterpret_cast<const char*>(&fileSize), 4);

    // Write 4 bytes of padding
    uint32_t padding = 0;
    outFile.write(reinterpret_cast<const char*>(&padding), 4);

    BitWriter writer(outFile);
    std::vector<uint8_t> window(0x800, 0x20);
    int windowPos = 0x7ef;

    for (size_t i = 0; i < buffer.size(); ++i) {
        // 简单起见，我们总是使用未压缩的字节
        writer.writeBits(1, 1);  // Uncompressed flag
        writer.writeBits(buffer[i], 8);  // Original byte

        window[windowPos] = buffer[i];
        windowPos = (windowPos + 1) & 0x7ff;
    }

    writer.flush();
    outFile.close();

    std::cout << "Compressed: " << inputPath.filename() << " -> " << outputPath.filename() << std::endl;
}

void processDirectory(const fs::path& inputDir, const fs::path& outputDir, bool isCompress) {
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".scp") {
            fs::path relativePath = fs::relative(entry.path(), inputDir);
            fs::path outputPath = outputDir / relativePath;
            fs::create_directories(outputPath.parent_path());

            // 检查文件大小
            std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
            std::streamsize size = file.tellg();
            file.close();

            if (size > 12) {
                if (isCompress) {
                    compressFile(entry.path(), outputPath);
                }
                else {
                    decompressFile(entry.path(), outputPath);
                }
            }
            else {
                std::cout << "Skipped (size <= 12 bytes): " << entry.path().filename() << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
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

    if (mode == "compress") {
        processDirectory(inputDir, outputDir, true);
    }
    else if (mode == "decompress") {
        processDirectory(inputDir, outputDir, false);
    }
    else {
        std::cerr << "Invalid mode. Use 'compress' or 'decompress'." << std::endl;
        return 1;
    }

    return 0;
}
