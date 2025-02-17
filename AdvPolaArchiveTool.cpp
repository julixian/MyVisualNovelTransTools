#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <cstdint>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct PackHeader {
    char magic[4] = { 'P', 'A', 'C', 'K' };
    uint32_t index_entries;
    uint32_t data_offset;
    uint32_t reserved = 0;
};

struct PackEntry {
    char name[16];
    uint32_t length;
    uint32_t offset;
};

struct PolaHeader {
    char magic[8] = { '*', 'P', 'o', 'l', 'a', '*', ' ', ' ' };
    uint32_t uncomprlen;
    uint32_t unknown[2] = { 0, 0 };
};
#pragma pack(pop)

// 处理单个文件并返回处理后的数据
std::vector<uint8_t> processFile(const fs::path& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << filepath << std::endl;
        return {};
    }

    // 读取文件大小
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < 0xC) {
        std::cerr << "文件太小: " << filepath << std::endl;
        return {};
    }

    // 读取0xB-0xC的uint16值
    file.seekg(0xB);
    uint16_t value;
    file.read(reinterpret_cast<char*>(&value), sizeof(uint16_t));

    // 创建结果vector并预分配空间
    std::vector<uint8_t> result(fileSize - 0xd + sizeof(PolaHeader));

    // 写入新的Pola头
    PolaHeader polaHeader;
    polaHeader.uncomprlen = value;
    std::memcpy(result.data(), &polaHeader, sizeof(PolaHeader));

    // 读取并复制文件剩余部分
    file.seekg(0xD);
    file.read(reinterpret_cast<char*>(result.data() + sizeof(PolaHeader)), fileSize - 0xD);

    return result;
}

void createPacFile(const std::string& folderPath, const std::string& outputPath) {
    std::vector<fs::path> files;
    std::vector<std::vector<uint8_t>> processedData;

    // 收集所有文件
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
            processedData.push_back(processFile(entry.path()));
        }
    }

    // 创建PAC文件
    std::ofstream pacFile(outputPath, std::ios::binary);
    if (!pacFile) {
        std::cerr << "无法创建PAC文件" << std::endl;
        return;
    }

    // 写入头部
    PackHeader header;
    header.index_entries = files.size();
    header.data_offset = sizeof(PackHeader) + files.size() * sizeof(PackEntry);

    pacFile.write(reinterpret_cast<char*>(&header), sizeof(header));

    // 计算每个文件的偏移
    uint32_t currentOffset = header.data_offset;
    std::vector<PackEntry> entries;

    // 写入索引
    for (size_t i = 0; i < files.size(); i++) {
        PackEntry entry;
        std::memset(entry.name, 0, sizeof(entry.name));
        std::string filename = files[i].filename().string();
        std::strncpy(entry.name, filename.c_str(), sizeof(entry.name) - 1);

        entry.length = processedData[i].size();
        entry.offset = currentOffset;

        entries.push_back(entry);
        currentOffset += entry.length;
    }

    // 写入所有索引项
    for (const auto& entry : entries) {
        pacFile.write(reinterpret_cast<const char*>(&entry), sizeof(PackEntry));
    }

    // 写入文件数据
    for (const auto& data : processedData) {
        pacFile.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    pacFile.close();
    std::cout << "PAC文件创建成功！" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Made by julixian 2025.02.17" << std::endl;
        std::cout << "用法: " << argv[0] << " <输入文件夹路径> <输出PAC文件路径>" << std::endl;
        return 1;
    }

    try {
        createPacFile(argv[1], argv[2]);
    }
    catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
