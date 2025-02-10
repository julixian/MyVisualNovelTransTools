#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

// 文件条目结构
struct FileEntry {
    std::string name;           // 13字节
    uint8_t reserved[7];        // 7字节保留
    uint32_t unpackedSize;      // 解压后大小
    uint32_t size;             // 压缩大小
    uint32_t offset;           // 相对偏移
};

class LaxExtractor {
private:
    std::ifstream file;
    std::vector<FileEntry> entries;

    bool readIndex() {
        // 读取并验证文件头
        char signature[10];
        file.read(signature, 10);
        if (memcmp(signature, "\x24\x5F\x41\x50\x5F\x24\x00\x00\x5F\x00", 10) != 0) {
            std::cerr << "Invalid signature!" << std::endl;
            return false;
        }

        // 读取文件数量
        uint16_t count;
        file.read((char*)&count, 2);

        // 读取数据区起始地址
        uint32_t dataOffset;
        file.read((char*)&dataOffset, 4);

        // 跳过8字节填充
        file.seekg(8, std::ios::cur);

        // 读取每个文件的索引
        for (uint16_t i = 0; i < count; ++i) {
            FileEntry entry;

            // 读取文件名(13字节)
            char name[14] = { 0 };  // 多一位用于字符串结尾
            file.read(name, 13);
            entry.name = name;

            // 读取保留字节(7字节)
            file.read((char*)entry.reserved, 7);

            // 读取大小和偏移信息
            file.read((char*)&entry.unpackedSize, 4);
            file.read((char*)&entry.size, 4);
            file.read((char*)&entry.offset, 4);

            // 计算实际偏移(相对于数据区起始位置)
            entry.offset += dataOffset;

            // 跳过2字节填充
            file.seekg(2, std::ios::cur);

            entries.push_back(entry);

            std::cout << "File: " << entry.name
                << "\nOffset: 0x" << std::hex << entry.offset
                << "\nSize: " << std::dec << entry.size
                << " -> " << entry.unpackedSize << " bytes\n" << std::endl;
        }

        return true;
    }

    bool extractFile(const FileEntry& entry, const fs::path& outPath) {
        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) return false;

        // 直接复制压缩数据
        std::vector<uint8_t> buffer(entry.size);
        file.seekg(entry.offset);
        file.read((char*)buffer.data(), entry.size);
        outFile.write((char*)buffer.data(), entry.size);

        return true;
    }

public:
    bool open(const std::string& filename) {
        file.open(filename, std::ios::binary);
        return file.is_open() && readIndex();
    }

    bool extract(const std::string& outDir) {
        if (!file.is_open()) return false;

        fs::create_directories(outDir);

        std::cout << "Found " << entries.size() << " files\n" << std::endl;

        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            fs::path outPath = fs::path(outDir) / entry.name;
            fs::create_directories(outPath.parent_path());

            std::cout << "[" << (i + 1) << "/" << entries.size() << "] "
                << entry.name << "\n"
                << "    Offset: 0x" << std::hex << std::setw(8) << std::setfill('0')
                << entry.offset << "\n"
                << "    Packed Size: " << std::dec << entry.size << " bytes\n"
                << "    Unpacked Size: " << entry.unpackedSize << " bytes"
                << std::endl;

            if (!extractFile(entry, outPath)) {
                std::cerr << "Failed to extract: " << entry.name << std::endl;
                continue;
            }
        }

        std::cout << "\nExtraction completed successfully" << std::endl;
        return true;
    }

    ~LaxExtractor() {
        if (file.is_open()) file.close();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Made by julixian 2025.02.10" << std::endl;
        std::cout << "Usage: " << argv[0] << " <input.lax> <output_dir>" << std::endl;
        return 1;
    }

    LaxExtractor extractor;
    if (!extractor.open(argv[1])) {
        std::cerr << "Failed to open LAX file" << std::endl;
        return 1;
    }

    if (!extractor.extract(argv[2])) {
        std::cerr << "Failed to extract files" << std::endl;
        return 1;
    }

    return 0;
}
