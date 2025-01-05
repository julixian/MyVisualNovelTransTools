#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <zlib.h>
#include <iomanip>
#include <cstdint>
#include <bitset>
#include <map>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

// 解包相关的类
class Entry {
public:
    std::string name;
    uint32_t offset;
    uint32_t size;
    uint32_t unpackedSize;
    bool isPacked;

    Entry(const std::string& n) : name(n), offset(0), size(0),
        unpackedSize(0), isPacked(false) {}

    Entry() : offset(0), size(0), unpackedSize(0), isPacked(false) {}
};

class PacArchive {
private:
    std::string filename;
    std::vector<Entry> directory;
    std::ifstream file;

    bool checkPlacement(uint32_t offset, uint32_t size, uint64_t maxOffset) {
        return (offset + size) <= maxOffset;
    }

    std::string decodeString(const std::vector<uint8_t>& buffer, size_t length) {
        return std::string(buffer.begin(), buffer.begin() + length);
    }

    bool createDirectory(const std::string& path) {
        try {
            fs::create_directories(path);
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error creating directory: " << e.what() << std::endl;
            return false;
        }
    }

    std::string getFileExtension(const std::string& filename) {
        size_t pos = filename.find_last_of('.');
        if (pos != std::string::npos) {
            return filename.substr(pos);
        }
        return "";
    }

    void printCompressionInfo(const std::vector<uint8_t>& buffer, const std::string& itemName) {
        if (buffer.size() < 2) {
            std::cout << "Buffer too small to contain zlib header for " << itemName << std::endl;
            return;
        }

        uint8_t cmf = buffer[0];
        uint8_t flg = buffer[1];

        std::cout << "Compression information for " << itemName << ":" << std::endl;

        // 压缩方法
        int cm = cmf & 0xF;
        std::cout << "  Compression method: " << (cm == 8 ? "deflate" : "unknown") << std::endl;

        // 压缩信息
        int cinfo = (cmf >> 4) & 0xF;
        std::cout << "  Compression info: " << cinfo << " (window size: " << (1 << (cinfo + 8)) << ")" << std::endl;

        // 检查标志
        bool fdict = (flg & 0x20) != 0;
        std::cout << "  Preset dictionary: " << (fdict ? "yes" : "no") << std::endl;

        // 压缩级别（近似值）
        int flevel = (flg >> 6) & 0x3;
        std::cout << "  Compression level (approximate): ";
        switch (flevel) {
        case 0: std::cout << "fastest"; break;
        case 1: std::cout << "fast"; break;
        case 2: std::cout << "default"; break;
        case 3: std::cout << "maximum"; break;
        }
        std::cout << std::endl;

        // 检查头部是否有效
        uint16_t check = (cmf << 8) | flg;
        std::cout << "  Header checksum: " << (check % 31 == 0 ? "valid" : "invalid") << std::endl;

        // 打印原始位
        std::cout << "  CMF bits: " << std::bitset<8>(cmf) << std::endl;
        std::cout << "  FLG bits: " << std::bitset<8>(flg) << std::endl;
    }

public:
    PacArchive(const std::string& fname) : filename(fname) {
        file.open(filename, std::ios::binary);
    }

    bool tryOpen(int32_t maxExtractFiles = -1) {
        if (!file.is_open()) {
            std::cerr << "Failed to open input file" << std::endl;
            return false;
        }

        int32_t totalCount;
        file.read(reinterpret_cast<char*>(&totalCount), sizeof(totalCount));
        if (totalCount <= 0 || totalCount > 10000) {
            std::cerr << "Invalid file count: " << totalCount << std::endl;
            return false;
        }

        std::cout << "Total files in archive: " << totalCount << std::endl;

        std::vector<uint8_t> nameBuffer(0x100);
        std::vector<uint8_t> zlibBuffer(0x100);

        directory.reserve(totalCount);

        for (int i = 0; i < totalCount; ++i) {
            int32_t nameLen, unpackedSize, packedSize;
            file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            file.read(reinterpret_cast<char*>(&unpackedSize), sizeof(unpackedSize));
            file.read(reinterpret_cast<char*>(&packedSize), sizeof(packedSize));

            if (nameLen <= 0 || nameLen > nameBuffer.size() ||
                unpackedSize < nameLen || unpackedSize > nameBuffer.size() ||
                packedSize <= 0 || packedSize > zlibBuffer.size()) {
                std::cerr << "Invalid size parameters at entry " << i << std::endl;
                return false;
            }

            file.read(reinterpret_cast<char*>(zlibBuffer.data()), packedSize);

            // 打印文件名压缩信息
            std::cout << "File name compression info for entry " << i << ":" << std::endl;
            printCompressionInfo(zlibBuffer, "File name");

            z_stream zs = { 0 };
            zs.zalloc = Z_NULL;
            zs.zfree = Z_NULL;
            zs.opaque = Z_NULL;
            zs.avail_in = packedSize;
            zs.next_in = zlibBuffer.data();
            zs.avail_out = nameBuffer.size();
            zs.next_out = nameBuffer.data();

            if (inflateInit(&zs) != Z_OK) {
                std::cerr << "Failed to initialize zlib for name decompression" << std::endl;
                return false;
            }

            if (inflate(&zs, Z_FINISH) != Z_STREAM_END) {
                inflateEnd(&zs);
                std::cerr << "Failed to decompress name data" << std::endl;
                return false;
            }

            inflateEnd(&zs);

            std::string name = decodeString(nameBuffer, nameLen);
            directory.push_back(Entry(name));
        }

        for (auto& entry : directory) {
            uint32_t offset, size, unpackedSize;
            file.read(reinterpret_cast<char*>(&offset), sizeof(offset));
            file.read(reinterpret_cast<char*>(&size), sizeof(size));
            file.read(reinterpret_cast<char*>(&unpackedSize), sizeof(unpackedSize));

            entry.offset = offset;
            entry.size = size;
            entry.unpackedSize = unpackedSize;
            entry.isPacked = true;

            if (!checkPlacement(offset, size, UINT64_MAX)) {
                std::cerr << "Invalid file placement for: " << entry.name << std::endl;
                return false;
            }
        }

        if (maxExtractFiles > 0 && maxExtractFiles < totalCount) {
            std::cout << "Limiting extraction to first " << maxExtractFiles << " files" << std::endl;
            directory.resize(maxExtractFiles);
        }

        return true;
    }

    bool extractEntry(const Entry& entry, const std::string& outputPath) {
        if (!file.is_open()) return false;

        fs::path fullPath = fs::path(outputPath) / entry.name;
        createDirectory(fullPath.parent_path().string());

        std::vector<uint8_t> buffer(entry.size);
        file.seekg(entry.offset);
        file.read(reinterpret_cast<char*>(buffer.data()), entry.size);

        std::ofstream outFile(fullPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create output file: " << fullPath << std::endl;
            return false;
        }

        printCompressionInfo(buffer, entry.name);

        if (!entry.isPacked) {
            outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            outFile.close();
            return true;
        }

        std::vector<uint8_t> unpackedData(entry.unpackedSize);
        z_stream zs = { 0 };
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = entry.size;
        zs.next_in = buffer.data();
        zs.avail_out = entry.unpackedSize;
        zs.next_out = unpackedData.data();

        if (inflateInit(&zs) != Z_OK) {
            std::cerr << "Failed to initialize zlib for data decompression" << std::endl;
            return false;
        }

        if (inflate(&zs, Z_FINISH) != Z_STREAM_END) {
            inflateEnd(&zs);
            std::cerr << "Failed to decompress file data" << std::endl;
            return false;
        }

        inflateEnd(&zs);

        outFile.write(reinterpret_cast<const char*>(unpackedData.data()), entry.unpackedSize);
        outFile.close();

        return true;
    }

    bool extractAll(const std::string& outputPath) {
        bool success = true;
        std::cout << "Extracting files to: " << outputPath << std::endl;

        for (const auto& entry : directory) {
            std::cout << "Extracting: " << entry.name << std::endl;
            if (!extractEntry(entry, outputPath)) {
                std::cerr << "Failed to extract: " << entry.name << std::endl;
                success = false;
            }
        }

        return success;
    }

    const std::vector<Entry>& getDirectory() const {
        return directory;
    }
};

// 封包相关的类
struct FileEntry {
    std::string name;
    uint32_t offset;
    uint32_t size;
    uint32_t unpackedSize;
    bool isPacked;
    std::vector<uint8_t> compressedNameData;
    int32_t compressedNameSize;
    int32_t nameLength;
    int32_t nameUnpackedSize;
};

class PacPacker {
private:
    std::string inputDir;
    std::string outputFile;
    std::string originalFile;
    std::vector<FileEntry> originalEntries;  // 存储原始文件信息
    int32_t numFilesToProcess;               // 要处理的文件数量

    // 从原始文件读取文件信息
    bool readOriginalEntries() {
        std::ifstream file(originalFile, std::ios::binary);
        if (!file) return false;

        // 读取文件数量
        int32_t totalCount;
        file.read(reinterpret_cast<char*>(&totalCount), sizeof(totalCount));

        // 读取所有文件名信息
        for (int i = 0; i < totalCount; ++i) {
            FileEntry entry;
            file.read(reinterpret_cast<char*>(&entry.nameLength), sizeof(entry.nameLength));
            file.read(reinterpret_cast<char*>(&entry.nameUnpackedSize), sizeof(entry.nameUnpackedSize));
            file.read(reinterpret_cast<char*>(&entry.compressedNameSize), sizeof(entry.compressedNameSize));

            // 读取压缩的文件名数据
            entry.compressedNameData.resize(entry.compressedNameSize);
            file.read(reinterpret_cast<char*>(entry.compressedNameData.data()), entry.compressedNameSize);

            // 解压文件名以获取实际名称
            std::vector<uint8_t> nameBuffer(entry.nameUnpackedSize);
            z_stream zs = { 0 };
            zs.next_in = entry.compressedNameData.data();
            zs.avail_in = entry.compressedNameSize;
            zs.next_out = nameBuffer.data();
            zs.avail_out = nameBuffer.size();

            if (inflateInit(&zs) != Z_OK) continue;
            if (inflate(&zs, Z_FINISH) != Z_STREAM_END) {
                inflateEnd(&zs);
                continue;
            }
            inflateEnd(&zs);

            entry.name = std::string(reinterpret_cast<char*>(nameBuffer.data()), entry.nameLength);
            originalEntries.push_back(entry);
        }

        // 读取文件元数据
        for (auto& entry : originalEntries) {
            file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
            file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
            file.read(reinterpret_cast<char*>(&entry.unpackedSize), sizeof(entry.unpackedSize));
            entry.isPacked = true;
        }

        return true;
    }

    // 压缩文件数据
    std::vector<uint8_t> compressData(const std::vector<uint8_t>& input) {
        z_stream zs = { 0 };
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;

        if (deflateInit2(&zs,
            Z_BEST_COMPRESSION,    // 最高压缩级别
            Z_DEFLATED,
            15,                    // 包含zlib头部
            8,                     // 内存级别
            Z_DEFAULT_STRATEGY) != Z_OK) {
            throw std::runtime_error("deflateInit failed");
        }

        size_t maxCompressedSize = compressBound(input.size());
        std::vector<uint8_t> output(maxCompressedSize);

        zs.next_in = const_cast<uint8_t*>(input.data());
        zs.avail_in = input.size();
        zs.next_out = output.data();
        zs.avail_out = output.size();

        if (deflate(&zs, Z_FINISH) != Z_STREAM_END) {
            deflateEnd(&zs);
            throw std::runtime_error("deflate failed");
        }

        output.resize(zs.total_out);
        deflateEnd(&zs);
        return output;
    }

    // 读取文件数据
    std::vector<uint8_t> readFileData(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open file: " + filepath);

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0);

        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    }

public:
    PacPacker(const std::string& inDir, const std::string& outFile, const std::string& origFile, int32_t numFiles)
        : inputDir(inDir), outputFile(outFile), originalFile(origFile), numFilesToProcess(numFiles) {}

    void createPacFile() {
        if (!readOriginalEntries()) {
            throw std::runtime_error("Failed to read original file entries");
        }

        std::cout << "Read " << originalEntries.size() << " entries from original file\n";

        // 打开原始文件和新文件
        std::ifstream origFile(originalFile, std::ios::binary);
        std::ofstream outFile(outputFile, std::ios::binary);
        if (!origFile || !outFile) {
            throw std::runtime_error("Cannot open files");
        }

        // 首先读取原始文件的所有数据
        origFile.seekg(0, std::ios::end);
        size_t totalSize = origFile.tellg();
        origFile.seekg(0);
        std::vector<char> originalData(totalSize);
        origFile.read(originalData.data(), totalSize);

        // 写入文件总数量（保持原始数量）
        int32_t totalCount = originalEntries.size();
        outFile.write(reinterpret_cast<char*>(&totalCount), sizeof(totalCount));

        // 写入所有文件名信息（使用原始压缩数据）
        for (const auto& entry : originalEntries) {
            outFile.write(reinterpret_cast<const char*>(&entry.nameLength), sizeof(entry.nameLength));
            outFile.write(reinterpret_cast<const char*>(&entry.nameUnpackedSize), sizeof(entry.nameUnpackedSize));
            outFile.write(reinterpret_cast<const char*>(&entry.compressedNameSize), sizeof(entry.compressedNameSize));
            outFile.write(reinterpret_cast<const char*>(entry.compressedNameData.data()), entry.compressedNameSize);
        }

        // 计算新的数据偏移位置
        uint32_t currentOffset = outFile.tellp();
        currentOffset += originalEntries.size() * 12;  // 为所有文件的元数据预留空间

        // 更新所有文件的偏移量
        std::vector<FileEntry> updatedEntries = originalEntries;  // 复制一份用于更新

        // 处理文件数据
        size_t dataStartPos = static_cast<size_t>(outFile.tellp()) + originalEntries.size() * 12;
        outFile.seekp(dataStartPos);

        // 处理要替换的文件
        for (size_t i = 0; i < originalEntries.size(); ++i) {
            if (i < numFilesToProcess) {
                // 尝试替换这个文件
                std::string fullPath = (fs::path(inputDir) / originalEntries[i].name).string();
                std::cout << "Processing: " << originalEntries[i].name << " from " << fullPath << std::endl;

                try {
                    // 尝试读取并压缩新文件
                    auto rawData = readFileData(fullPath);
                    auto compressedData = compressData(rawData);

                    // 更新文件信息
                    updatedEntries[i].offset = currentOffset;
                    updatedEntries[i].size = compressedData.size();
                    updatedEntries[i].unpackedSize = rawData.size();

                    // 写入新的压缩数据
                    outFile.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
                    currentOffset += compressedData.size();

                    std::cout << "Successfully replaced: " << originalEntries[i].name << std::endl;
                }
                catch (const std::exception& e) {
                    // 如果读取或压缩失败，作为未替换文件处理
                    std::cout << "Failed to process " << originalEntries[i].name << ": " << e.what() << std::endl;
                    std::cout << "Using original data instead." << std::endl;

                    // 复制原始数据
                    updatedEntries[i].offset = currentOffset;
                    size_t originalOffset = originalEntries[i].offset;
                    size_t originalSize = originalEntries[i].size;

                    if (originalOffset + originalSize <= originalData.size()) {
                        outFile.write(&originalData[originalOffset], originalSize);
                        currentOffset += originalSize;
                    }
                    else {
                        throw std::runtime_error("Invalid original file offset or size");
                    }
                }
            }
            else {
                // 对于未替换的文件，直接复制原始数据
                updatedEntries[i].offset = currentOffset;

                // 直接从原始数据中复制
                size_t originalOffset = originalEntries[i].offset;
                size_t originalSize = originalEntries[i].size;

                if (originalOffset + originalSize <= originalData.size()) {
                    outFile.write(&originalData[originalOffset], originalSize);
                    currentOffset += originalSize;
                }
                else {
                    throw std::runtime_error("Invalid original file offset or size");
                }
            }
        }

        // 回到元数据位置写入更新后的偏移量
        outFile.seekp(dataStartPos - originalEntries.size() * 12);

        // 写入更新后的所有文件元数据
        for (const auto& entry : updatedEntries) {
            outFile.write(reinterpret_cast<const char*>(&entry.offset), sizeof(entry.offset));
            outFile.write(reinterpret_cast<const char*>(&entry.size), sizeof(entry.size));
            outFile.write(reinterpret_cast<const char*>(&entry.unpackedSize), sizeof(entry.unpackedSize));
        }
    }




    void verifyFile() {
        std::ifstream origFile(originalFile, std::ios::binary);
        std::ifstream newFile(outputFile, std::ios::binary);

        if (!origFile || !newFile) {
            std::cout << "Cannot open files for verification\n";
            return;
        }

        // 获取文件大小
        origFile.seekg(0, std::ios::end);
        newFile.seekg(0, std::ios::end);
        size_t origSize = origFile.tellg();
        size_t newSize = newFile.tellg();

        std::cout << "\nFile size comparison:\n";
        std::cout << "Original size: " << origSize << " bytes\n";
        std::cout << "New file size: " << newSize << " bytes\n";

        // 重置文件指针并比较内容
        origFile.seekg(0);
        newFile.seekg(0);

        bool identical = true;
        char bo, bn;
        size_t pos = 0;
        while (origFile.get(bo) && newFile.get(bn)) {
            if (bo != bn) {
                if (identical) {
                    //std::cout << "First difference at position: " << pos << "\n";
                    identical = false;
                }
            }
            pos++;
        }

        if (identical) {
            std::cout << "Files are completely identical!\n";
        }
    }
};

void printUsage(const char* programName) {
    std::cout << "Made by julixian 2025.01.01" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "To unpack: " << programName << " unpack <pac_file> <output_directory> <number_of_files>" << std::endl;
    std::cout << "To pack: " << programName << " pack <input_directory> <output_pac> <original_pac> <number_of_files>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string operation = argv[1];

    if (operation == "unpack") {
        if (argc < 5) {
            printUsage(argv[0]);
            return 1;
        }

        std::string inputFile = argv[2];
        std::string outputDir = argv[3];
        int32_t numFiles = std::stoi(argv[4]);

        PacArchive archive(inputFile);
        if (!archive.tryOpen(numFiles)) {
            std::cerr << "Failed to open archive: " << inputFile << std::endl;
            return 1;
        }

        std::cout << "Archive opened successfully" << std::endl;

        if (!archive.extractAll(outputDir)) {
            std::cerr << "Some files failed to extract" << std::endl;
            return 1;
        }

        std::cout << "Extraction completed successfully" << std::endl;
    }
    else if (operation == "pack") {
        if (argc < 6) {
            printUsage(argv[0]);
            return 1;
        }

        try {
            int32_t numFiles = std::stoi(argv[5]);
            PacPacker packer(argv[2], argv[3], argv[4], numFiles);

            std::cout << "Creating PAC file...\n";
            packer.createPacFile();

            std::cout << "Verifying file...\n";
            packer.verifyFile();
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Unknown operation: " << operation << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
