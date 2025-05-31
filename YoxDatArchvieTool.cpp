#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <zlib.h>
#include <memory>
#include <algorithm>

namespace fs = std::filesystem;

// 统一Entry结构体
struct Entry {
    uint32_t offset;
    uint32_t size;
    uint32_t extra1;    // 用于封包时保存额外的8字节数据
    uint32_t extra2;
    std::string name;
    uint32_t unpackedSize;
    bool isPacked;
};

// 通用函数
uint32_t ReadUInt32(std::ifstream& file) {
    uint32_t value = 0;
    file.read(reinterpret_cast<char*>(&value), 4);
    return value;
}

void WriteUInt32(std::ofstream& file, uint32_t value) {
    file.write(reinterpret_cast<const char*>(&value), 4);
}

// 解压函数
std::vector<uint8_t> DecompressZLib(const std::vector<uint8_t>& input, uint32_t unpackedSize) {
    std::vector<uint8_t> output(unpackedSize);
    z_stream strm = {};

    if (inflateInit(&strm) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib");
    }

    strm.next_in = const_cast<uint8_t*>(input.data());
    strm.avail_in = input.size();
    strm.next_out = output.data();
    strm.avail_out = unpackedSize;

    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Failed to decompress data");
    }

    return output;
}

// 压缩函数
std::vector<uint8_t> CompressZLib(const std::vector<uint8_t>& input) {
    uLong compressedSize = compressBound(input.size());
    std::vector<uint8_t> output(compressedSize);

    z_stream strm = {};
    if (deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib");
    }

    strm.next_in = const_cast<uint8_t*>(input.data());
    strm.avail_in = input.size();
    strm.next_out = output.data();
    strm.avail_out = compressedSize;

    deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    output.resize(strm.total_out);
    return output;
}

void ExtractYoxDat(const std::string& inputPath, const std::string& outputPath, int version) {
    std::ifstream file(inputPath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open input file" << std::endl;
        return;
    }

    // 读取文件头
    uint32_t signature = ReadUInt32(file);
    if (signature != 0x584F59) { // "YOX"
        std::cerr << "Invalid YOX signature" << std::endl;
        return;
    }

    file.seekg(8);
    uint32_t indexOffset = ReadUInt32(file);
    uint32_t count = ReadUInt32(file);

    std::cout << "File count: " << count << std::endl;

    // 创建输出目录
    fs::create_directories(outputPath);

    // 读取索引
    std::vector<Entry> entries;
    file.seekg(indexOffset);
    bool newVersion = version >= 2;

    // 尝试8字节条目格式
    for (uint32_t i = 0; i < count; ++i) {
        Entry entry;
        entry.offset = ReadUInt32(file);
        entry.size = ReadUInt32(file);
        entry.isPacked = false;
        entry.name = std::to_string(i);
        while (entry.name.length() < 5) entry.name = "0" + entry.name;
        entries.push_back(entry);
        if (newVersion) {
            file.seekg(8, std::ios::cur);
        }
    }

    // 处理每个文件
    for (auto& entry : entries) {
        file.seekg(entry.offset);

        // 检查是否为压缩文件
        uint32_t entrySignature = ReadUInt32(file);
        if (entrySignature == 0x584F59) {
            uint32_t flags = ReadUInt32(file);
            if (flags & 2) {
                entry.isPacked = true;
                entry.unpackedSize = ReadUInt32(file);
                entry.offset += 0x10;
                entry.size -= 0x10;
            }
        }

        // 读取文件数据
        file.seekg(entry.offset);
        std::vector<uint8_t> data(entry.size);
        file.read(reinterpret_cast<char*>(data.data()), entry.size);

        // 如果是压缩文件，进行解压
        if (entry.isPacked) {
            try {
                data = DecompressZLib(data, entry.unpackedSize);
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to decompress file " << entry.name << ": " << e.what() << std::endl;
                continue;
            }
        }

        // 写入文件
        std::string outPath = outputPath + "/" + entry.name;
        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create output file: " << entry.name << std::endl;
            continue;
        }

        outFile.write(reinterpret_cast<char*>(data.data()), data.size());
        std::cout << "Extracted: " << entry.name << (entry.isPacked ? " (Decompressed)" : "") << std::endl;
    }

    std::cout << "Extraction completed!" << std::endl;
}

void PackYoxDat(const std::string& originalDat, const std::string& inputDir, const std::string& outputDat, int version, bool zlib) {
    // 读取原始DAT文件的头部和索引
    std::ifstream origFile(originalDat, std::ios::binary);
    if (!origFile) {
        throw std::runtime_error("Failed to open original DAT file");
    }

    // 读取头部16字节
    std::vector<uint8_t> header(16);
    origFile.read(reinterpret_cast<char*>(header.data()), 16);

    // 获取文件数量和原始索引偏移
    origFile.seekg(0xC);
    uint32_t fileCount = ReadUInt32(origFile);
    origFile.seekg(0x8);
    uint32_t origIndexOffset = ReadUInt32(origFile);

    // 读取原始索引
    std::vector<Entry> entries;
    origFile.seekg(origIndexOffset);
    bool newVersion = version >= 2;

    for (uint32_t i = 0; i < fileCount; ++i) {
        Entry entry;
        entry.offset = ReadUInt32(origFile);
        entry.size = ReadUInt32(origFile);
        if (newVersion) {
            entry.extra1 = ReadUInt32(origFile);
            entry.extra2 = ReadUInt32(origFile);
        }
        entry.name = std::to_string(i);
        while (entry.name.length() < 5) entry.name = "0" + entry.name;
        entries.push_back(entry);
    }

    size_t sundrySize = fs::file_size(originalDat) - origFile.tellg();
    //std::cout << "sundrySize: " << std::hex << sundrySize << std::endl;
    std::vector<uint8_t> sundryIndex(sundrySize);
    origFile.read((char*)sundryIndex.data(), sundrySize);

    // 创建输出文件
    std::ofstream outFile(outputDat, std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Failed to create output file");
    }

    // 写入头部
    outFile.write(reinterpret_cast<const char*>(header.data()), 16);

    // 当前写入位置（从0x800开始）
    uint32_t currentOffset = 0x800;
    outFile.seekp(currentOffset);

    // 处理每个文件
    for (auto& entry : entries) {
        std::string inPath = inputDir + "/" + entry.name;
        std::ifstream inFile(inPath, std::ios::binary);
        if (!inFile) {
            throw std::runtime_error("Failed to open input file: " + inPath);
        }

        // 读取文件内容
        inFile.seekg(0, std::ios::end);
        size_t fileSize = inFile.tellg();
        inFile.seekg(0);
        std::vector<uint8_t> fileData(fileSize);
        inFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);

        // 压缩数据
        std::vector<uint8_t> finalData;
        if (zlib) {
            finalData = CompressZLib(fileData);
            WriteUInt32(outFile, 0x584F59);  // "YOX"
            WriteUInt32(outFile, 2);         // 压缩标志
            WriteUInt32(outFile, fileSize);  // 解压后大小
            WriteUInt32(outFile, 0);         // 保留
        }
        else {
            finalData = fileData;
            //WriteUInt32(outFile, 0x584F59);  // "YOX"
            //WriteUInt32(outFile, 0);         // 压缩标志
            //WriteUInt32(outFile, fileSize);  // 解压后大小
            //WriteUInt32(outFile, 0);         // 保留
        }

        // 写入压缩数据
        outFile.write(reinterpret_cast<const char*>(finalData.data()), finalData.size());

        // 更新索引信息
        entry.offset = currentOffset;
        entry.size = finalData.size();
        if (zlib) {
            entry.size += 16;
        }
        entry.unpackedSize = fileSize;

        // 计算下一个0x800对齐的位置
        currentOffset += entry.size;
        if (currentOffset % 0x800 != 0) {
            currentOffset = (currentOffset + 0x800) & ~0x7FF;
        }
        outFile.seekp(currentOffset);
        //std::cout << "C" << std::endl;
    }

    // 写入索引表
    uint32_t indexOffset = currentOffset;
    for (const auto& entry : entries) {
        WriteUInt32(outFile, entry.offset);
        WriteUInt32(outFile, entry.size);
        if (newVersion) {
            WriteUInt32(outFile, entry.extra1);
            WriteUInt32(outFile, entry.extra2);
        }
    }
    outFile.write((char*)sundryIndex.data(), sundryIndex.size());

    // 更新头部中的索引偏移
    outFile.seekp(0x8);
    WriteUInt32(outFile, indexOffset);

    std::cout << "Packing completed successfully!" << std::endl;
}

void PrintUsage(const char* programName) {
    std::cout << "Made by julixian 2025.03.17" << std::endl;
    std::cout << "Usage:\n"
        << "Extract: " << programName << " -e <version> <input.dat> <output_directory>\n"
        << "Pack:    " << programName << " -p <version> [--zlib] <original.dat> <input_directory> <output.dat>\n"
        << "version: 1 or 2" << "\n"
        << "--zlib: use zlib compress when repacking, usually used for script.dat" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    int version = std::stoi(std::string(argv[2]));
    bool zlib = mode == "-p" && std::string(argv[3]) == "--zlib";

    if (mode == "-e") {
        // 解包模式
        ExtractYoxDat(argv[argc - 2], argv[argc - 1], version);
    }
    else if (mode == "-p") {
        // 封包模式
        PackYoxDat(argv[argc - 3], argv[argc - 2], argv[argc - 1], version, zlib);
    }
    else {
        PrintUsage(argv[0]);
        return 1;
    }

    return 0;
}
