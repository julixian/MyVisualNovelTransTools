#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// File entry structure
struct Entry {
    std::string name;
    uint32_t offset;
    uint32_t size;
    uint32_t unpacked_size;
    bool is_packed;
};

// BitStream class for reading compressed data
class BitStream {
private:
    std::ifstream& m_input;
    uint32_t m_bits;
    int m_bit_count;

public:
    BitStream(std::ifstream& input) : m_input(input), m_bits(0), m_bit_count(0) {}

    void fetchBits() {
        m_input.read(reinterpret_cast<char*>(&m_bits), 4);
        m_bit_count = 32;
    }

    int getNextBit() {
        if (0 == m_bit_count)
            fetchBits();
        int bit = (m_bits >> 31) & 1;
        m_bits <<= 1;
        --m_bit_count;
        return bit;
    }

    int getBits(int count) {
        uint32_t bits = 0;
        for (int i = 0; i < count; ++i) {
            bits = (bits << 1) | getNextBit();
        }
        return bits;
    }
};

// FA2 decompression class
class Fa2Decompressor {
private:
    std::ifstream& m_input;
    std::vector<uint8_t> m_output;

public:
    Fa2Decompressor(std::ifstream& input, uint32_t unpacked_size)
        : m_input(input), m_output(unpacked_size) {}

    std::vector<uint8_t> unpack() {
        BitStream bits(m_input);
        size_t dst = 0;

        while (dst < m_output.size()) {
            if (bits.getNextBit() != 0) {
                uint8_t byte;
                m_input.read(reinterpret_cast<char*>(&byte), 1);
                m_output[dst++] = byte;
                continue;
            }

            int offset;
            if (bits.getNextBit() != 0) {
                if (bits.getNextBit() != 0) {
                    uint8_t byte;
                    m_input.read(reinterpret_cast<char*>(&byte), 1);
                    offset = byte << 3;
                    offset |= bits.getBits(3);
                    offset += 0x100;
                    if (offset >= 0x8FF)
                        break;
                }
                else {
                    uint8_t byte;
                    m_input.read(reinterpret_cast<char*>(&byte), 1);
                    offset = byte;
                }
                m_output[dst] = m_output[dst - offset - 1];
                m_output[dst + 1] = m_output[dst - offset];
                dst += 2;
            }
            else {
                if (bits.getNextBit() != 0) {
                    uint8_t byte;
                    m_input.read(reinterpret_cast<char*>(&byte), 1);
                    offset = byte << 1;
                    offset |= bits.getNextBit();
                }
                else {
                    offset = 0x100;
                    if (bits.getNextBit() != 0) {
                        uint8_t byte;
                        m_input.read(reinterpret_cast<char*>(&byte), 1);
                        offset |= byte;
                        offset <<= 1;
                        offset |= bits.getNextBit();
                    }
                    else if (bits.getNextBit() != 0) {
                        uint8_t byte;
                        m_input.read(reinterpret_cast<char*>(&byte), 1);
                        offset |= byte;
                        offset <<= 2;
                        offset |= bits.getBits(2);
                    }
                    else if (bits.getNextBit() != 0) {
                        uint8_t byte;
                        m_input.read(reinterpret_cast<char*>(&byte), 1);
                        offset |= byte;
                        offset <<= 3;
                        offset |= bits.getBits(3);
                    }
                    else {
                        uint8_t byte;
                        m_input.read(reinterpret_cast<char*>(&byte), 1);
                        offset |= byte;
                        offset <<= 4;
                        offset |= bits.getBits(4);
                    }
                }

                int count;
                if (bits.getNextBit() != 0) {
                    count = 3;
                }
                else if (bits.getNextBit() != 0) {
                    count = 4;
                }
                else if (bits.getNextBit() != 0) {
                    count = 5 + bits.getNextBit();
                }
                else if (bits.getNextBit() != 0) {
                    count = 7 + bits.getBits(2);
                }
                else if (bits.getNextBit() != 0) {
                    count = 11 + bits.getBits(4);
                }
                else {
                    uint8_t byte;
                    m_input.read(reinterpret_cast<char*>(&byte), 1);
                    count = 27 + byte;
                }

                // Copy overlapped data
                for (int i = 0; i < count; ++i) {
                    m_output[dst + i] = m_output[dst - offset - 1 + i];
                }
                dst += count;
            }
        }
        return m_output;
    }
};

// FA2 file extractor class
class Fa2Extractor {
private:
    std::ifstream m_file;
    std::vector<Entry> m_entries;

    std::vector<uint8_t> decompress(uint32_t unpacked_size) {
        Fa2Decompressor decompressor(m_file, unpacked_size);
        return decompressor.unpack();
    }

    bool readIndex(uint32_t index_offset, bool is_packed, int count) {
        m_file.seekg(index_offset);

        std::vector<uint8_t> index;
        if (is_packed) {
            // 直接用count * 0x20作为解压大小
            index = decompress(count * 0x20);
        }
        else {
            // 读取到文件末尾
            m_file.seekg(0, std::ios::end);
            size_t file_size = m_file.tellg();
            size_t index_size = file_size - index_offset;
            m_file.seekg(index_offset);

            index.resize(index_size);
            m_file.read(reinterpret_cast<char*>(index.data()), index_size);
        }

        // 解析索引
        size_t pos = 0;
        uint32_t data_offset = 0x10;

        for (int i = 0; i < count; ++i) {
            Entry entry;

            // 读取文件名 (15字节)
            char name[16] = { 0 };
            std::memcpy(name, &index[pos], 15);
            entry.name = name;
            pos += 15;

            // 读取压缩标志
            entry.is_packed = (index[pos] & 2) != 0;
            pos += 9;

            // 读取大小信息
            entry.unpacked_size = *reinterpret_cast<uint32_t*>(&index[pos]);
            entry.size = *reinterpret_cast<uint32_t*>(&index[pos + 4]);
            pos += 8;

            entry.offset = data_offset;
            data_offset += (entry.size + 0xF) & ~0xF;

            m_entries.push_back(entry);
        }

        return true;
    }

public:
    bool open(const std::string& filename) {
        m_file.open(filename, std::ios::binary);
        if (!m_file.is_open())
            return false;

        // 读取文件头
        uint32_t signature;
        m_file.read(reinterpret_cast<char*>(&signature), 4);
        if (signature != 0x00324146) // "FA2\0"
            return false;

        uint8_t flags;
        m_file.read(reinterpret_cast<char*>(&flags), 1);
        bool is_packed = (flags & 1) != 0;

        m_file.seekg(8);
        uint32_t index_offset;
        m_file.read(reinterpret_cast<char*>(&index_offset), 4);

        int count;
        m_file.read(reinterpret_cast<char*>(&count), 4);
        if (count <= 0 || count > 10000) // 简单的合法性检查
            return false;

        return readIndex(index_offset, is_packed, count);
    }

    bool extractFile(const Entry& entry, const std::string& output_path) {
        m_file.seekg(entry.offset);

        std::vector<uint8_t> data;
        if (entry.is_packed) {
            data = decompress(entry.unpacked_size);
        }
        else {
            data.resize(entry.size);
            m_file.read(reinterpret_cast<char*>(data.data()), entry.size);
        }

        std::ofstream outFile(output_path + "/" + entry.name, std::ios::binary);
        if (!outFile.is_open())
            return false;

        outFile.write(reinterpret_cast<char*>(data.data()),
            entry.is_packed ? entry.unpacked_size : entry.size);
        return true;
    }

    const std::vector<Entry>& getEntries() const {
        return m_entries;
    }

    ~Fa2Extractor() {
        if (m_file.is_open())
            m_file.close();
    }
};

// FA2 packer class
class FA2Packer {
private:
    std::vector<Entry> entries;
    std::ofstream output;
    uint32_t dataOffset = 0x10;  // Data section starts at 0x10

    void writeUint32(uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    }

    void writePadding(size_t count) {
        std::vector<char> padding(count, 0);
        output.write(padding.data(), count);
    }

    void writeFileHeader(uint32_t indexOffset, uint32_t fileCount) {
        output.seekp(0);
        writeUint32(0x00324146);  // 'FA2\0'
        output.put(0x00);  // Index not compressed
        writePadding(3);   // 0x05-0x07 set to 0x00
        writeUint32(indexOffset);
        writeUint32(fileCount);
    }

    void writeFileData(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            std::cerr << "Cannot open file: " << filePath << std::endl;
            return;
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(fileSize);
        file.read(buffer.data(), fileSize);
        output.write(buffer.data(), fileSize);

        // 16-byte alignment
        size_t padding = (16 - (fileSize % 16)) % 16;
        writePadding(padding);
    }

    void writeIndexEntry(const Entry& entry) {
        // File name (15 bytes)
        output.write(entry.name.c_str(), std::min(entry.name.length(), size_t(14)));
        writePadding(15 - std::min(entry.name.length(), size_t(14)));

        // Flag byte (not compressed)
        output.put(0x00);

        // 8 bytes padding
        writePadding(8);

        // Unpacked size (same as actual size, as not compressed)
        writeUint32(entry.size);

        // Packed size (same as actual size, as not compressed)
        writeUint32(entry.size);
    }

public:
    void packDirectory(const std::string& dirPath, const std::string& outputPath) {
        output.open(outputPath, std::ios::binary);
        if (!output) {
            std::cerr << "Cannot create output file: " << outputPath << std::endl;
            return;
        }

        // Reserve space for file header
        writePadding(0x10);

        // Write file data
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                Entry fileEntry;
                fileEntry.name = entry.path().filename().string();
                fileEntry.offset = dataOffset;
                fileEntry.size = fs::file_size(entry.path());
                fileEntry.unpacked_size = fileEntry.size;
                fileEntry.is_packed = false;

                writeFileData(entry.path().string());
                entries.push_back(fileEntry);

                dataOffset = output.tellp();
            }
        }

        // Write index
        uint32_t indexOffset = dataOffset;
        for (const auto& entry : entries) {
            writeIndexEntry(entry);
        }

        // Write file header
        writeFileHeader(indexOffset, entries.size());

        output.close();
        std::cout << "Packing completed: " << outputPath << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.01.14" << std::endl;
        std::cout << "Usage: " << argv[0] << " <mode> <input> <output>\n";
        std::cout << "Mode: pack or unpack\n";
        std::cout << "For pack: <input> is a directory, <output> is the .fa2 file\n";
        std::cout << "For unpack: <input> is the .fa2 file, <output> is the output directory\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string input = argv[2];
    std::string output = argv[3];

    if (mode == "pack") {
        FA2Packer packer;
        packer.packDirectory(input, output);
    }
    else if (mode == "unpack") {
        Fa2Extractor extractor;
        if (!extractor.open(input)) {
            std::cout << "Failed to open FA2 file\n";
            return 1;
        }

        for (const auto& entry : extractor.getEntries()) {
            std::cout << "Extracting: " << entry.name << std::endl;
            if (!extractor.extractFile(entry, output)) {
                std::cout << "Failed to extract: " << entry.name << std::endl;
            }
        }
    }
    else {
        std::cout << "Invalid mode. Use 'pack' or 'unpack'.\n";
        return 1;
    }

    return 0;
}
