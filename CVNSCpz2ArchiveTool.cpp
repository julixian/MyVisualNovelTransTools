#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <random>

namespace fs = std::filesystem;

const uint32_t SIGNATURE = 0x325A5043; // 'CPZ2'
const uint32_t ENCRYPTION_TABLE[] = {
    0x3A68CDBF, 0xD3C3A711, 0x8414876E, 0x657BEFDB, 0xCDD7C125, 0x09328580, 0x288FFEDD, 0x99EBF13A,
    0x5A471F95, 0x1EA3F4F1, 0xF4FF524E, 0xD358E8A9, 0xC5B71015, 0xA913046F, 0x2D6FD2BD, 0x68C8BE19
};

struct FileEntry {
    std::string name;
    uint32_t size;
    uint32_t offset;
    uint32_t key;
};

uint32_t rotateLeft(uint32_t value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

void encryptData(std::vector<uint8_t>& data, uint32_t key) {
    int shift = 5;
    int k = key;
    for (int i = 0; i < 8; ++i) {
        shift ^= k & 0xF;
        k >>= 4;
    }
    shift += 8;

    uint32_t* data32 = reinterpret_cast<uint32_t*>(data.data());
    int table_ptr = 0;
    for (size_t i = 0; i < data.size() / 4; ++i) {
        uint32_t t = rotateLeft(data32[i], shift);
        data32[i] = (t + 0x15C3E7u) ^ (ENCRYPTION_TABLE[table_ptr++ & 0xF] + key);
    }

    uint8_t* data8 = reinterpret_cast<uint8_t*>(data32 + data.size() / 4);
    shift = 0;
    for (size_t i = 0; i < data.size() % 4; ++i) {
        data8[i] = ((data8[i] - 0x37) ^ ((ENCRYPTION_TABLE[table_ptr++ & 0xF] + key) >> shift));
        shift += 4;
    }
}

void createCpz(const std::string& inputDir, const std::string& cpzPath) {
    std::vector<FileEntry> entries;
    uint32_t totalSize = 0;

    // 收集文件信息
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            FileEntry fileEntry;
            fileEntry.name = fs::relative(entry.path(), inputDir).string();
            fileEntry.size = fs::file_size(entry.path());
            fileEntry.offset = totalSize;
            fileEntry.key = std::random_device()(); // 生成随机密钥
            entries.push_back(fileEntry);
            totalSize += fileEntry.size;
        }
    }

    // 创建索引
    std::vector<uint8_t> index;
    uint32_t indexOffset = 0;
    for (auto& entry : entries) {
        uint32_t entrySize = 0x18 + entry.name.length() + 1; // +1 for null terminator
        index.resize(index.size() + entrySize);

        *reinterpret_cast<uint32_t*>(&index[indexOffset]) = entrySize;
        *reinterpret_cast<uint32_t*>(&index[indexOffset + 4]) = entry.size;
        *reinterpret_cast<uint32_t*>(&index[indexOffset + 8]) = entry.offset;
        *reinterpret_cast<uint32_t*>(&index[indexOffset + 0x14]) = entry.key ^ 0x796C3AFDu;
        std::strcpy(reinterpret_cast<char*>(&index[indexOffset + 0x18]), entry.name.c_str());

        indexOffset += entrySize;
    }

    // 加密索引
    uint32_t indexKey = std::random_device()();
    encryptData(index, indexKey);

    // 写入CPZ文件
    std::ofstream cpzFile(cpzPath, std::ios::binary);

    // 写入头部
    uint32_t fileCount = entries.size() ^ 0xE47C59F3;
    uint32_t indexSize = index.size() ^ 0x3F71DE2Au;
    uint32_t encryptedIndexKey = indexKey ^ 0x40DE832Cu;

    cpzFile.write(reinterpret_cast<const char*>(&SIGNATURE), 4);
    cpzFile.write(reinterpret_cast<const char*>(&fileCount), 4);
    cpzFile.write(reinterpret_cast<const char*>(&indexSize), 4);
    cpzFile.write("\0\0\0\0", 4); // 未使用的4字节
    cpzFile.write(reinterpret_cast<const char*>(&encryptedIndexKey), 4);

    // 写入索引
    cpzFile.write(reinterpret_cast<const char*>(index.data()), index.size());

    // 写入文件数据
    for (const auto& entry : entries) {
        std::ifstream inputFile(fs::path(inputDir) / entry.name, std::ios::binary);
        std::vector<uint8_t> fileData(entry.size);
        inputFile.read(reinterpret_cast<char*>(fileData.data()), entry.size);
        encryptData(fileData, entry.key);
        cpzFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
        std::cout << "已打包: " << entry.name << std::endl;
    }

    cpzFile.close();
    std::cout << "CPZ文件创建完成: " << cpzPath << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Made by julixian 2025.01.31" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <input_dir> <output_cpz_file>" << std::endl;
        return 1;
    }

    std::string inputDir = argv[1];
    std::string cpzPath = argv[2];

    createCpz(inputDir, cpzPath);

    return 0;
}
