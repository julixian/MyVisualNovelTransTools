#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <zlib.h>
#include <windows.h>
#include <map>

namespace fs = std::filesystem;

// 解密表
const unsigned char DefaultNameKey[256] = {
    0x00, 0x16, 0xC9, 0x4A, 0x91, 0x04, 0x5E, 0x20, 0x33, 0x14, 0x4B, 0x8A, 0x0A, 0x70, 0x9F, 0x36,
    0xAF, 0x0D, 0x93, 0xB0, 0x2B, 0xFE, 0x29, 0x72, 0x94, 0x99, 0x9B, 0xED, 0xCE, 0xC4, 0xF1, 0xF4,
    0x9C, 0x1B, 0xE0, 0x02, 0x87, 0x82, 0x47, 0xDF, 0xF3, 0xA9, 0xDC, 0xEF, 0x3B, 0xB9, 0xC5, 0x83,
    0xD8, 0x0F, 0x9A, 0xE2, 0xBD, 0x28, 0x9E, 0xAB, 0xB7, 0x3F, 0x75, 0x63, 0x2A, 0x5D, 0x05, 0x4E,
    0x1F, 0xCF, 0x61, 0xAA, 0x10, 0x77, 0xCC, 0x90, 0xD5, 0x43, 0xA6, 0xEC, 0x88, 0x08, 0x97, 0x7A,
    0xF5, 0x42, 0xBA, 0x3A, 0xF6, 0x7D, 0x8F, 0xB5, 0x18, 0x76, 0x40, 0x6D, 0xAC, 0x19, 0x1D, 0x4D,
    0x38, 0x03, 0x8C, 0x01, 0x7B, 0xE7, 0xB3, 0x2F, 0x67, 0xF8, 0x6A, 0x13, 0xAD, 0xF0, 0x5B, 0x7C,
    0x24, 0x6B, 0xC6, 0xC0, 0x06, 0x89, 0x71, 0xDD, 0x23, 0x11, 0x09, 0x1A, 0xF9, 0xC2, 0x31, 0xB1,
    0xBE, 0x8B, 0x6E, 0xFA, 0x48, 0x52, 0xDA, 0x17, 0x21, 0xD9, 0x60, 0x78, 0xA4, 0xA8, 0x26, 0x79,
    0x5C, 0x41, 0xBF, 0xD4, 0x3C, 0x1E, 0x86, 0xA7, 0x6F, 0xB8, 0x2C, 0xD2, 0x57, 0x56, 0x58, 0x66,
    0xE4, 0xCA, 0x55, 0x44, 0xE8, 0x85, 0x53, 0x96, 0x7F, 0x68, 0xC7, 0x73, 0x4C, 0xE6, 0x12, 0xB6,
    0x98, 0xBC, 0xAE, 0xEE, 0xA0, 0xFC, 0x69, 0x62, 0xC1, 0xE3, 0xB2, 0x95, 0xE9, 0x46, 0xCD, 0xD0,
    0x50, 0x15, 0x9D, 0x51, 0x30, 0x5A, 0x64, 0xF7, 0x8E, 0x07, 0xBB, 0xC8, 0xA2, 0x3E, 0xD3, 0x39,
    0xA5, 0x49, 0x5F, 0x3D, 0xD1, 0xCB, 0x0E, 0x54, 0xC3, 0x4F, 0x8D, 0x84, 0xDB, 0x2D, 0x0B, 0xD7,
    0x92, 0x7E, 0xE1, 0xEB, 0x81, 0xFD, 0x25, 0xEA, 0x2E, 0xB4, 0xD6, 0x37, 0xA1, 0xE5, 0x6C, 0x1C,
    0x22, 0x45, 0xF2, 0x65, 0x74, 0x34, 0x35, 0xDE, 0x59, 0x27, 0xA3, 0xFB, 0x0C, 0x80, 0x32, 0xFF,
};

// 合并的结构体定义
struct FltEntry {
    std::wstring name;
    uint32_t compression;
    bool isEncrypted;
    uint32_t size;
    uint32_t unpackedSize;
    int64_t offset;
    std::vector<uint8_t> indexData; // 用于封包

    uint8_t getKey() const {
        uint64_t key = offset;
        return (uint8_t)(key ^ (key >> 8) ^ (key >> 16) ^ (key >> 24)
            ^ (key >> 32) ^ (key >> 40) ^ (key >> 48) ^ (key >> 56));
    }
};

struct PackHeader {
    uint32_t signature;    // 'LIB_'
    char     packdata[12]; // "PACKDATA0000"
    uint32_t unknown1;
    uint32_t fileCount;
    uint32_t unknown2;
    uint32_t isEncrypted;
    std::vector<uint8_t> padding;
};

// 进度条类定义
class ProgressBar {
    int width;
    int total;
    int current;
    std::string task;

public:
    ProgressBar(int width = 50, const std::string& task = "Progress")
        : width(width), total(100), current(0), task(task) {}

    void setTotal(int t) { total = t; }
    void setCurrent(int c) { current = c; }
    void setTask(const std::string& t) { task = t; }

    void update(int current_value) {
        current = current_value;
        display();
    }

    void display() const {
        float progress = static_cast<float>(current) / total;
        int pos = static_cast<int>(width * progress);

        std::cout << "\r" << task << ": [";
        for (int i = 0; i < width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << "% "
            << "(" << current << "/" << total << ")"
            << std::flush;
    }

    void done() const {
        std::cout << std::endl;
    }
};

// 保持所有原有函数不变
void decryptIndex(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = DefaultNameKey[data[i]];
    }
}

std::vector<FltEntry> readIndex(std::ifstream& file) {
    // 检查文件头
    char signature[4];
    file.read(signature, 4);
    if (*(uint32_t*)signature != 0x5F42494C) { // 'LIB_'
        throw std::runtime_error("Invalid signature");
    }

    char packdata[12];
    file.read(packdata, 12);
    if (memcmp(packdata, "PACKDATA0000", 11) != 0) {
        throw std::runtime_error("Invalid packdata signature");
    }

    // 读取文件数量和加密标志
    file.seekg(0x14);
    int32_t count;
    file.read((char*)&count, 4);

    file.seekg(0x1C);
    int32_t isEncrypted;
    file.read((char*)&isEncrypted, 4);

    // 读取索引数据
    file.seekg(0x100);
    std::vector<uint8_t> indexData(count * 0x100);
    file.read((char*)indexData.data(), indexData.size());

    // 如果需要，解密索引
    if (isEncrypted) {
        decryptIndex(indexData);
    }

    // 将解密后的索引保存到文件
    std::ofstream indexFile("index_decrypted.bin", std::ios::binary);
    indexFile.write((char*)indexData.data(), indexData.size());
    indexFile.close();

    // 解析索引
    std::vector<FltEntry> entries;
    for (int i = 0; i < count; ++i) {
        size_t basePos = i * 0x100;
        FltEntry entry;

        // 读取文件名
        int nameLen = 0;
        while (nameLen < 0xE8) {
            if (indexData[basePos + nameLen] == 0 && indexData[basePos + nameLen + 1] == 0)
                break;
            nameLen += 2;
        }
        if (nameLen == 0) continue;

        entry.name = std::wstring((wchar_t*)&indexData[basePos], nameLen / 2);
        entry.compression = indexData[basePos + 0xEA];
        entry.isEncrypted = indexData[basePos + 0xEB] == 1;
        entry.size = *(uint32_t*)&indexData[basePos + 0xF0];
        entry.unpackedSize = *(uint32_t*)&indexData[basePos + 0xF4];
        entry.offset = *(int64_t*)&indexData[basePos + 0xF8];

        entries.push_back(entry);
    }

    return entries;
}

std::vector<uint8_t> extractFile(std::ifstream& file, const FltEntry& entry) {
    // 读取原始数据
    file.seekg(entry.offset);
    std::vector<uint8_t> data(entry.size);
    file.read((char*)data.data(), entry.size);

    // 解密
    if (entry.isEncrypted) {
        uint8_t key = entry.getKey();
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] ^= key;
        }
    }

    // 解压
    if (entry.compression == 1) {
        std::vector<uint8_t> unpackedData(entry.unpackedSize);
        z_stream strm = {};
        inflateInit(&strm);

        strm.next_in = data.data();
        strm.avail_in = data.size();
        strm.next_out = unpackedData.data();
        strm.avail_out = unpackedData.size();

        if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
            throw std::runtime_error("Decompression failed");
        }

        inflateEnd(&strm);
        return unpackedData;
    }

    return data;
}

std::pair<PackHeader, std::vector<FltEntry>> readOriginalPac(const std::string& pacPath, const std::string& decryptedIndexPath) {
    PackHeader header;
    std::vector<FltEntry> entries;

    // 从原PAC读取文件头
    std::ifstream pacFile(pacPath, std::ios::binary);
    if (!pacFile) {
        throw std::runtime_error("Cannot open original PAC file");
    }

    // 读取文件头
    pacFile.read((char*)&header.signature, 4);
    pacFile.read(header.packdata, 12);
    pacFile.read((char*)&header.unknown1, 4);
    pacFile.read((char*)&header.fileCount, 4);
    pacFile.read((char*)&header.unknown2, 4);
    pacFile.read((char*)&header.isEncrypted, 4);

    // 保存填充数据
    header.padding.resize(0xE0);
    pacFile.read((char*)header.padding.data(), 0xE0);

    // 从解密的索引文件读取索引数据
    std::ifstream indexFile(decryptedIndexPath, std::ios::binary);
    if (!indexFile) {
        throw std::runtime_error("Cannot open decrypted index file");
    }

    // 读取索引
    for (uint32_t i = 0; i < header.fileCount; ++i) {
        FltEntry entry;

        // 读取解密后的索引数据
        entry.indexData.resize(0x100);
        indexFile.read((char*)entry.indexData.data(), 0x100);

        // 解析索引数据
        int nameLen = 0;
        while (nameLen < 0xE8) {
            if (entry.indexData[nameLen] == 0 && entry.indexData[nameLen + 1] == 0)
                break;
            nameLen += 2;
        }

        entry.name = std::wstring((wchar_t*)entry.indexData.data(), nameLen / 2);
        entry.compression = entry.indexData[0xEA];
        entry.isEncrypted = entry.indexData[0xEB] == 1;
        entry.size = *(uint32_t*)&entry.indexData[0xF0];
        entry.unpackedSize = *(uint32_t*)&entry.indexData[0xF4];
        entry.offset = *(int64_t*)&entry.indexData[0xF8];

        entries.push_back(entry);
    }

    return { header, entries };
}

std::vector<uint8_t> compressData(const std::vector<uint8_t>& input) {
    z_stream strm = {};
    deflateInit(&strm, Z_BEST_COMPRESSION);

    std::vector<uint8_t> output(deflateBound(&strm, input.size()));

    strm.next_in = (Bytef*)input.data();
    strm.avail_in = input.size();
    strm.next_out = output.data();
    strm.avail_out = output.size();

    deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    output.resize(strm.total_out);
    return output;
}

void createNewPac(const std::string& originalPacPath,
    const std::string& decryptedIndexPath,
    const std::string& inputDir,
    const std::string& outputPacPath) {
    std::cout << "Starting to create new PAC file...\n\n";

    // 读取原始PAC文件的头和索引
    auto [header, entries] = readOriginalPac(originalPacPath, decryptedIndexPath);

    // 创建新PAC文件
    std::ofstream outFile(outputPacPath, std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Cannot create output PAC file");
    }

    // 写入文件头
    std::cout << "Writing header...\n";
    header.isEncrypted = 0;
    outFile.write((char*)&header.signature, 4);
    outFile.write(header.packdata, 12);
    outFile.write((char*)&header.unknown1, 4);
    outFile.write((char*)&header.fileCount, 4);
    outFile.write((char*)&header.unknown2, 4);
    outFile.write((char*)&header.isEncrypted, 4);
    outFile.write((char*)header.padding.data(), header.padding.size());

    // 计算文件数据起始位置
    int64_t currentOffset = 0x100 + header.fileCount * 0x100;

    // 第一遍：处理所有文件
    ProgressBar progressCompress(50, "Compressing files");
    progressCompress.setTotal(entries.size());

    std::map<std::wstring, std::pair<std::vector<uint8_t>, uint32_t>> processedFiles;
    for (size_t i = 0; i < entries.size(); ++i) {
        auto& entry = entries[i];
        fs::path inputPath = fs::path(inputDir) / entry.name;

        // 显示当前处理的文件
        std::wcout << L"\nProcessing: " << entry.name << std::endl;

        // 读取输入文件
        std::ifstream inFile(inputPath, std::ios::binary);
        if (!inFile) {
            throw std::runtime_error("Cannot open input file: " + inputPath.string());
        }

        // 读取文件数据
        std::vector<uint8_t> fileData(
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>()
        );

        // 压缩数据
        std::vector<uint8_t> compressedData = compressData(fileData);

        // 更新条目信息
        entry.compression = 1;
        entry.isEncrypted = 0;
        entry.unpackedSize = fileData.size();
        entry.size = compressedData.size();
        entry.offset = currentOffset;

        // 保存处理后的数据
        processedFiles[entry.name] = { std::move(compressedData), fileData.size() };

        // 更新偏移量
        currentOffset += entry.size;

        // 更新进度
        progressCompress.update(i + 1);
    }
    progressCompress.done();

    // 写入索引
    std::cout << "\nWriting index...\n";
    ProgressBar progressIndex(50, "Writing index");
    progressIndex.setTotal(entries.size());

    for (size_t i = 0; i < entries.size(); ++i) {
        auto& entry = entries[i];
        // 更新索引数据
        entry.indexData[0xEA] = entry.compression;
        entry.indexData[0xEB] = entry.isEncrypted;
        *(uint32_t*)&entry.indexData[0xF0] = entry.size;
        *(uint32_t*)&entry.indexData[0xF4] = entry.unpackedSize;
        *(int64_t*)&entry.indexData[0xF8] = entry.offset;

        outFile.write((char*)entry.indexData.data(), 0x100);
        progressIndex.update(i + 1);
    }
    progressIndex.done();

    // 写入文件数据
    std::cout << "\nWriting file data...\n";
    ProgressBar progressWrite(50, "Writing files");
    progressWrite.setTotal(entries.size());

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const auto& [compressedData, _] = processedFiles[entry.name];
        outFile.write((char*)compressedData.data(), compressedData.size());
        progressWrite.update(i + 1);
    }
    progressWrite.done();

    // 显示完成信息
    std::cout << "\nNew PAC file created successfully!\n";
    std::cout << "Total files processed: " << entries.size() << "\n";
}

// 解包函数
void extractPac(const std::string& inputPath, const std::string& outputDir) {
    // 创建输出目录
    fs::create_directories(outputDir);

    // 打开文件
    std::ifstream file(inputPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open input file");
    }

    // 读取索引
    auto entries = readIndex(file);

    // 提取文件
    for (const auto& entry : entries) {
        std::cout << "Extracting: " << std::string(entry.name.begin(), entry.name.end()) << std::endl;

        // 创建输出路径
        fs::path outPath = fs::path(outputDir) / entry.name;
        fs::create_directories(outPath.parent_path());

        // 提取并保存文件
        auto data = extractFile(file, entry);
        std::ofstream outFile(outPath, std::ios::binary);
        outFile.write((char*)data.data(), data.size());
    }

    std::cout << "Extraction complete!\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Made by julixian 2025.01.26" << std::endl;
        std::cout << "Usage:\n"
            << "Extract: " << argv[0] << " -e <input.pac> <output_dir>\n"
            << "Pack:    " << argv[0] << " -p <original.pac> <index_decrypted.bin> <input_dir> <output.pac>\n";
        return 1;
    }

    try {
        std::string mode = argv[1];
        if (mode == "-e") {
            if (argc != 4) {
                std::cout << "Extract usage: " << argv[0] << " -e <input.pac> <output_dir>\n";
                return 1;
            }
            extractPac(argv[2], argv[3]);
        }
        else if (mode == "-p") {
            if (argc != 6) {
                std::cout << "Pack usage: " << argv[0] << " -p <original.pac> <index_decrypted.bin> <input_dir> <output.pac>\n";
                return 1;
            }
            createNewPac(argv[2], argv[3], argv[4], argv[5]);
        }
        else {
            std::cout << "Invalid mode. Use -e for extract or -p for pack.\n";
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
