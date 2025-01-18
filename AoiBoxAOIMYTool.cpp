#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <memory>
#include <algorithm>

namespace fs = std::filesystem;

// 文件条目结构
struct Entry {
    std::wstring name;
    uint32_t offset;
    uint32_t size;
};

// 大端字节序转换
uint32_t BigEndian(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x000000FF) << 24);
}

// 密钥生成
uint8_t KeyFromOffset(uint32_t offset) {
    const uint32_t BASE = 0x5CC8E9D7;
    uint32_t v1 = offset - BASE + (0xA3371629u >> ((offset & 0xF) + 1)) - (BASE << (31 - (offset & 0xF)));

    uint32_t v3 = v1 << (31 - ((offset >> 4) & 0xF));
    uint32_t v4 = v1 >> (((offset >> 4) & 0xF) + 1);
    uint32_t v5 = offset - BASE
        + ((offset - BASE + v3 + v4) << (31 - ((offset >> 8) & 0xF)))
        + ((offset - BASE + v3 + v4) >> (((offset >> 8) & 0xF) + 1));
    uint32_t v6 = offset - BASE
        + (v5 << (31 - ((offset >> 12) & 0xF))) + (v5 >> (((offset >> 12) & 0xF) + 1));
    uint32_t v7 = offset - BASE
        + (v6 << (31 - ((offset >> 16) & 0xF))) + (v6 >> (((offset >> 16) & 0xF) + 1));
    int v8 = (offset >> 20) & 0xF;
    uint32_t v9 = offset - BASE
        + ((offset - BASE + (v7 << (31 - v8)) + (v7 >> (v8 + 1))) << (31 - ((offset >> 24) & 0xF)))
        + ((offset - BASE + (v7 << (31 - v8)) + (v7 >> (v8 + 1))) >> (((offset >> 24) & 0xF) + 1));
    uint32_t key = (offset - BASE + (v9 << (31 - (offset >> 28))) + (v9 >> ((offset >> 28) + 1))) >> (offset & 0xF);
    return static_cast<uint8_t>(key);
}

// 解包函数
bool ExtractBox(const fs::path& boxPath, const fs::path& outputDir) {
    std::ifstream file(boxPath, std::ios::binary);
    if (!file) {
        std::wcerr << L"无法打开文件: " << boxPath.wstring() << std::endl;
        return false;
    }

    // 读取并检查文件头
    std::vector<wchar_t> header(8);
    file.read(reinterpret_cast<char*>(header.data()), 16);
    std::wstring headerStr(header.data());
    if (headerStr != L"AOIMY01\0") {
        std::wcerr << L"无效的文件格式" << std::endl;
        return false;
    }

    // 读取文件数量
    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), 4);
    count = BigEndian(count);

    // 跳过保留的4字节
    file.seekg(0x18);

    // 读取文件条目
    std::vector<Entry> entries;
    for (uint32_t i = 0; i < count; ++i) {
        Entry entry;

        // 读取文件名（32字节）
        std::vector<char> nameBuffer(32);
        file.read(nameBuffer.data(), 32);
        entry.name = std::wstring(reinterpret_cast<wchar_t*>(nameBuffer.data()));

        // 读取偏移和大小
        uint32_t beOffset, beSize;
        file.read(reinterpret_cast<char*>(&beOffset), 4);
        file.read(reinterpret_cast<char*>(&beSize), 4);
        entry.offset = BigEndian(beOffset);
        entry.size = BigEndian(beSize);

        entries.push_back(entry);
    }

    // 创建输出目录
    fs::create_directories(outputDir);

    // 提取文件
    for (const auto& entry : entries) {
        std::wcout << L"正在提取: " << entry.name << std::endl;

        // 读取文件数据
        std::vector<uint8_t> data(entry.size);
        file.seekg(entry.offset);
        file.read(reinterpret_cast<char*>(data.data()), entry.size);

        // 解密数据
        uint32_t offset = entry.offset;
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] ^= KeyFromOffset(offset++);
        }

        // 写入文件
        fs::path outPath = outputDir / entry.name;
        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) {
            std::wcerr << L"无法创建文件: " << outPath.wstring() << std::endl;
            continue;
        }
        outFile.write(reinterpret_cast<char*>(data.data()), data.size());
    }

    return true;
}

// 封包函数
bool CreateBox(const fs::path& inputDir, const fs::path& boxPath) {
    // 收集文件信息
    std::vector<Entry> entries;
    uint32_t currentOffset = 0;

    // 计算索引区大小
    currentOffset = 0x18;

    // 收集目录中的所有文件
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;

        Entry fileEntry;
        fileEntry.name = entry.path().filename().wstring();
        fileEntry.size = static_cast<uint32_t>(entry.file_size());
        entries.push_back(fileEntry);
    }

    // 计算每个文件的偏移
    currentOffset += static_cast<uint32_t>(entries.size() * 0x28);
    for (auto& entry : entries) {
        entry.offset = currentOffset;
        currentOffset += entry.size;
    }

    // 创建box文件
    std::ofstream boxFile(boxPath, std::ios::binary);
    if (!boxFile) {
        std::wcerr << L"无法创建文件: " << boxPath.wstring() << std::endl;
        return false;
    }

    // 写入文件头
    const wchar_t header[] = L"AOIMY01\0";
    boxFile.write(reinterpret_cast<const char*>(header), 16);

    // 写入文件数量（大端字节序）
    uint32_t count = BigEndian(static_cast<uint32_t>(entries.size()));
    boxFile.write(reinterpret_cast<char*>(&count), 4);

    // 写入保留的4字节
    uint32_t reserved = 0;
    boxFile.write(reinterpret_cast<char*>(&reserved), 4);

    // 写入文件条目
    for (const auto& entry : entries) {
        // 写入文件名（32字节，不足部分填充0）
        std::vector<char> nameBuffer(32, 0);
        memcpy(nameBuffer.data(), entry.name.c_str(),
            std::min(entry.name.length() * sizeof(wchar_t), size_t(32)));
        boxFile.write(nameBuffer.data(), 32);

        // 写入偏移和大小（大端字节序）
        uint32_t beOffset = BigEndian(entry.offset);
        uint32_t beSize = BigEndian(entry.size);
        boxFile.write(reinterpret_cast<char*>(&beOffset), 4);
        boxFile.write(reinterpret_cast<char*>(&beSize), 4);
    }

    // 写入文件数据
    for (const auto& entry : entries) {
        std::wcout << L"正在处理: " << entry.name << std::endl;

        // 读取源文件
        std::ifstream inFile(inputDir / entry.name, std::ios::binary);
        if (!inFile) {
            std::wcerr << L"无法读取文件: " << entry.name << std::endl;
            continue;
        }

        // 读取文件数据
        std::vector<uint8_t> data(entry.size);
        inFile.read(reinterpret_cast<char*>(data.data()), entry.size);

        // 加密数据
        uint32_t offset = entry.offset;
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] ^= KeyFromOffset(offset++);
        }

        // 写入加密后的数据
        boxFile.write(reinterpret_cast<char*>(data.data()), data.size());
    }

    return true;
}

void PrintUsage(const char* programName) {
    std::cout << "Made by julixian 2025.01.17" << std::endl;
    std::cout << "Usage:\n"
        << "extract: " << programName << " -e <box_file> <output_dir>\n"
        << "repack: " << programName << " -r <input_dir> <output_box_file>\n";
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    fs::path path1 = argv[2];
    fs::path path2 = argv[3];

    if (mode == "-e") {
        // 解包模式
        if (!fs::exists(path1)) {
            std::cout << "Box文件不存在" << std::endl;
            return 1;
        }
        if (ExtractBox(path1, path2)) {
            std::cout << "解包完成" << std::endl;
        }
        else {
            std::cout << "解包失败" << std::endl;
            return 1;
        }
    }
    else if (mode == "-r") {
        // 封包模式
        if (!fs::exists(path1) || !fs::is_directory(path1)) {
            std::cout << "输入目录不存在或不是目录" << std::endl;
            return 1;
        }
        if (CreateBox(path1, path2)) {
            std::cout << "封包完成" << std::endl;
        }
        else {
            std::cout << "封包失败" << std::endl;
            return 1;
        }
    }
    else {
        PrintUsage(argv[0]);
        return 1;
    }

    return 0;
}
