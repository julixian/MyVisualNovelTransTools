#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <zlib.h>
#include <cstring>

namespace fs = std::filesystem;

// 文件条目结构 - 解包用
struct ExtractFileEntry {
    uint16_t compression;    // 压缩类型
    std::string name;        // 文件名
    uint32_t offset;         // 文件数据偏移
    uint32_t size;           // 压缩后大小
    uint32_t unpackedSize;   // 解压后大小
};

// 文件条目结构 - 封包用
struct PackFileEntry {
    uint16_t compression;    // 压缩类型 (0=不压缩, 2=zlib压缩)
    std::string name;        // 文件名
    uint32_t nameOffset;     // 文件名在字符串表中的偏移
    uint32_t offset;         // 文件数据偏移
    uint32_t size;           // 压缩后大小
    uint32_t unpackedSize;   // 解压后大小
    std::vector<uint8_t> data; // 压缩后的数据
};

// 读取指定偏移的uint16_t
uint16_t ReadUInt16(std::ifstream& file, uint32_t offset) {
    file.seekg(offset);
    uint16_t value;
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

// 读取指定偏移的uint32_t
uint32_t ReadUInt32(std::ifstream& file, uint32_t offset) {
    file.seekg(offset);
    uint32_t value;
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

// 读取以null结尾的字符串
std::string ReadString(std::ifstream& file, uint32_t offset) {
    file.seekg(offset);
    std::string result;
    char c;
    while (file.get(c) && c != '\0') {
        result.push_back(c);
    }
    return result;
}

// 解压缩zlib数据
bool DecompressZlib(const std::vector<uint8_t>& input, std::vector<uint8_t>& output, uint32_t unpackedSize) {
    output.resize(unpackedSize);

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = static_cast<uInt>(input.size());
    strm.next_in = const_cast<Bytef*>(input.data());
    strm.avail_out = static_cast<uInt>(output.size());
    strm.next_out = output.data();

    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        std::cerr << "zlib初始化失败: " << ret << std::endl;
        return false;
    }

    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        std::cerr << "zlib解压失败: " << ret << std::endl;
        return false;
    }

    return true;
}

// 压缩数据使用zlib
bool CompressData(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    // 预估压缩后的最大大小
    uLong compressedBufferSize = compressBound(input.size());
    output.resize(compressedBufferSize);

    // 压缩数据
    uLongf destLen = compressedBufferSize;
    int result = compress2(output.data(), &destLen, input.data(), input.size(), Z_BEST_COMPRESSION);

    if (result != Z_OK) {
        std::cerr << "压缩失败: " << result << std::endl;
        return false;
    }

    // 调整输出大小为实际压缩后的大小
    output.resize(destLen);
    return true;
}

// 提取文件
bool ExtractFile(std::ifstream& archive, const ExtractFileEntry& entry, const fs::path& outputDir) {
    // 创建输出目录
    fs::create_directories(outputDir);
    fs::path outputPath = outputDir / entry.name;

    // 创建输出文件的父目录
    fs::create_directories(outputPath.parent_path());

    // 读取压缩数据
    std::vector<uint8_t> compressedData(entry.size);
    archive.seekg(entry.offset);
    archive.read(reinterpret_cast<char*>(compressedData.data()), entry.size);

    // 打开输出文件
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "无法创建输出文件: " << outputPath << std::endl;
        return false;
    }

    // 如果文件被压缩，则解压
    if (entry.compression > 1) {
        std::vector<uint8_t> uncompressedData;
        if (!DecompressZlib(compressedData, uncompressedData, entry.unpackedSize)) {
            std::cerr << "解压文件失败: " << entry.name << std::endl;
            return false;
        }
        outFile.write(reinterpret_cast<const char*>(uncompressedData.data()), uncompressedData.size());
    }
    else {
        // 直接写入未压缩数据
        outFile.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
    }

    std::cout << "已提取: " << entry.name << " (" << (entry.compression > 1 ? "已解压" : "未压缩") << ")" << std::endl;
    return true;
}

// 解析并提取FX封包
bool ExtractFxArchive(const std::string& archivePath, const std::string& outputDir) {
    std::ifstream file(archivePath, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << archivePath << std::endl;
        return false;
    }

    // 检查文件头
    char header[10];
    file.read(header, 9);
    header[9] = '\0';
    if (std::string(header) != "PARROT1.0") {
        std::cerr << "无效的文件格式，不是Scoop归档文件" << std::endl;
        return false;
    }

    // 读取文件条目数量
    uint16_t entryCount = ReadUInt16(file, 0x0A);
    std::cout << "文件条目数量: " << entryCount << std::endl;

    // 读取索引区大小
    uint32_t indexSize = ReadUInt32(file, 0x0C);
    std::cout << "索引区大小: " << indexSize << " 字节" << std::endl;

    // 解析文件条目
    std::vector<ExtractFileEntry> entries;
    uint32_t indexOffset = 0x10;

    for (int i = 0; i < entryCount; ++i) {
        ExtractFileEntry entry;
        entry.compression = ReadUInt16(file, indexOffset);
        uint32_t nameOffset = ReadUInt32(file, indexOffset + 2);
        entry.name = ReadString(file, nameOffset);
        entry.offset = ReadUInt32(file, indexOffset + 6);
        entry.size = ReadUInt32(file, indexOffset + 0x0A);
        entry.unpackedSize = ReadUInt32(file, indexOffset + 0x0E);

        entries.push_back(entry);
        indexOffset += 0x12;
    }

    // 提取文件
    for (const auto& entry : entries) {
        if (!ExtractFile(file, entry, outputDir)) {
            std::cerr << "提取文件失败: " << entry.name << std::endl;
        }
    }

    std::cout << "提取完成，共 " << entries.size() << " 个文件" << std::endl;
    return true;
}

// 收集要打包的文件
std::vector<PackFileEntry> CollectFiles(const fs::path& inputDir, bool useCompression) {
    std::vector<PackFileEntry> entries;

    // 递归遍历目录
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        PackFileEntry fileEntry;

        // 获取相对路径作为文件名
        fs::path relativePath = fs::relative(entry.path(), inputDir);
        fileEntry.name = relativePath.string();

        std::replace(fileEntry.name.begin(), fileEntry.name.end(), '/', '\\');

        // 读取文件内容
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file) {
            std::cerr << "无法读取文件: " << entry.path() << std::endl;
            continue;
        }

        // 获取文件大小
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // 读取文件内容
        std::vector<uint8_t> fileContent(fileSize);
        file.read(reinterpret_cast<char*>(fileContent.data()), fileSize);

        fileEntry.unpackedSize = fileSize;

        // 根据设置决定是否压缩
        if (useCompression && fileSize > 0) {
            fileEntry.compression = 2; // zlib压缩
            if (!CompressData(fileContent, fileEntry.data)) {
                std::cerr << "压缩文件失败: " << fileEntry.name << std::endl;
                fileEntry.compression = 0;
                fileEntry.data = fileContent;
            }
        }
        else {
            fileEntry.compression = 0; // 不压缩
            fileEntry.data = fileContent;
        }

        fileEntry.size = fileEntry.data.size();

        entries.push_back(fileEntry);
        std::cout << "添加文件: " << fileEntry.name
            << " (原始大小: " << fileEntry.unpackedSize
            << ", 压缩后: " << fileEntry.size
            << ", 压缩率: " << (fileEntry.unpackedSize > 0 ?
                (100 - fileEntry.size * 100 / fileEntry.unpackedSize) : 0)
            << "%)" << std::endl;
    }

    return entries;
}

// 创建FX封包
bool CreateFxArchive(const std::string& outputPath, std::vector<PackFileEntry>& entries) {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        std::cerr << "无法创建输出文件: " << outputPath << std::endl;
        return false;
    }

    // 写入文件头 "PARROT1.0"
    file.write("PARROT1.0\0", 0xA);

    // 写入文件条目数量 (2字节)
    uint16_t entryCount = static_cast<uint16_t>(entries.size());
    file.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

    std::cout << entryCount << std::endl;

    // 计算索引区大小 (包括文件名字符串)
    uint32_t indexSize = 0;
    uint32_t stringTableOffset = 0x10 + entries.size() * 0x12; // 头部(0x10) + 每个条目(0x12)

    // 构建字符串表和计算偏移
    std::vector<PackFileEntry> entriesWithOffsets = std::move(entries);
    std::vector<uint8_t> stringTable;

    for (auto& entry : entriesWithOffsets) {
        entry.nameOffset = (uint32_t)(stringTableOffset + stringTable.size());
        stringTable.insert(stringTable.end(), entry.name.begin(), entry.name.end());
        stringTable.push_back(0x00); // 字符串以null结尾
    }

    indexSize = stringTableOffset + stringTable.size();

    // 写入索引区大小 (4字节)
    file.write(reinterpret_cast<const char*>(&indexSize), sizeof(indexSize));

    // 计算数据区开始位置
    uint32_t dataOffset = stringTableOffset + stringTable.size();

    // 更新每个文件的数据偏移
    uint32_t currentOffset = dataOffset;
    for (auto& entry : entriesWithOffsets) {
        entry.offset = currentOffset;
        currentOffset += entry.size;
    }

    // 写入文件条目信息
    for (const auto& entry : entriesWithOffsets) {
        // 写入压缩类型 (2字节)
        file.write(reinterpret_cast<const char*>(&entry.compression), sizeof(entry.compression));

        // 写入文件名偏移 (4字节)
        file.write(reinterpret_cast<const char*>(&entry.nameOffset), sizeof(entry.nameOffset));

        // 写入文件数据偏移 (4字节)
        file.write(reinterpret_cast<const char*>(&entry.offset), sizeof(entry.offset));

        // 写入压缩后大小 (4字节)
        file.write(reinterpret_cast<const char*>(&entry.size), sizeof(entry.size));

        // 写入解压后大小 (4字节)
        file.write(reinterpret_cast<const char*>(&entry.unpackedSize), sizeof(entry.unpackedSize));
    }

    // 写入字符串表
    file.write((char*)stringTable.data(), stringTable.size());

    // 写入文件数据
    for (const auto& entry : entriesWithOffsets) {
        file.write(reinterpret_cast<const char*>(entry.data.data()), entry.data.size());
    }

    std::cout << "封包创建成功: " << outputPath << std::endl;
    std::cout << "总文件数: " << entriesWithOffsets.size() << std::endl;
    std::cout << "索引区大小: " << indexSize << " 字节" << std::endl;
    std::cout << "总文件大小: " << file.tellp() << " 字节" << std::endl;

    return true;
}

// 显示帮助信息
void ShowHelp(const char* programName) {
    std::cout << "Scoop FX 封包工具" << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  解包: " << programName << " -e <FX文件路径> <输出目录>" << std::endl;
    std::cout << "  封包: " << programName << " -c <输入目录> <输出FX文件> [--no-compress]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -e, --extract    解包模式" << std::endl;
    std::cout << "  -c, --create     封包模式" << std::endl;
    std::cout << "  --no-compress    封包时不压缩文件" << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << programName << " -e game.fx extracted" << std::endl;
    std::cout << "  " << programName << " -c input_folder output.fx" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        ShowHelp(argv[0]);
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "-e" || mode == "--extract") {
        // 解包模式
        std::string archivePath = argv[2];
        std::string outputDir = argv[3];

        if (!ExtractFxArchive(archivePath, outputDir)) {
            std::cerr << "提取失败" << std::endl;
            return 1;
        }
    }
    else if (mode == "-c" || mode == "--create") {
        // 封包模式
        std::string inputDir = argv[2];
        std::string outputPath = argv[3];

        // 检查是否使用压缩
        bool useCompression = true;
        if (argc > 4 && std::string(argv[4]) == "--no-compress") {
            useCompression = false;
            std::cout << "已禁用压缩" << std::endl;
        }

        // 检查输入目录是否存在
        if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
            std::cerr << "输入目录不存在或不是一个目录: " << inputDir << std::endl;
            return 1;
        }

        // 收集要打包的文件
        std::vector<PackFileEntry> entries = CollectFiles(inputDir, useCompression);

        if (entries.empty()) {
            std::cerr << "没有找到要打包的文件" << std::endl;
            return 1;
        }

        // 创建封包
        if (!CreateFxArchive(outputPath, entries)) {
            std::cerr << "创建封包失败" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "未知模式: " << mode << std::endl;
        ShowHelp(argv[0]);
        return 1;
    }

    return 0;
}
