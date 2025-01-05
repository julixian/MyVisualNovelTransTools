#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct TcdHeader {
    uint32_t signature;  // 'TCD3'
    uint32_t fileCount;
};

struct TcdSection {
    uint32_t dataSize;
    uint32_t indexOffset;
    int32_t dirCount;
    int32_t dirNameLength;
    int32_t fileCount;
    int32_t fileNameLength;
    uint32_t padding[2];    // 补齐到0x20字节
};

struct TcdDirEntry {
    int32_t fileCount;
    int32_t namesOffset;
    int32_t firstIndex;
    int32_t padding;
};
#pragma pack(pop)

class TcdManager {
private:
    const char* extensions[5] = { ".TCT", ".TSF", ".SPD", ".OGG", ".WAV" };
    std::fstream file;
    TcdHeader header;
    std::vector<TcdSection> sections;

    bool readHeader() {
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        return (header.signature == 0x33444354); // 'TCD3'
    }

    bool readSections() {
        sections.clear();
        for (int i = 0; i < 5; i++) {
            TcdSection section;
            file.read(reinterpret_cast<char*>(&section), sizeof(section));
            if (section.indexOffset != 0) {
                sections.push_back(section);
            }
        }
        return !sections.empty();
    }

    std::string getName(std::vector<uint8_t>& names, int nameLength, int& offset) {
        char buffer[256] = { 0 };
        memcpy(buffer, &names[offset], nameLength);
        offset += nameLength;
        return std::string(buffer);
    }

    void decryptNames(std::vector<uint8_t>& buffer, uint8_t key) {
        for (auto& byte : buffer) {
            byte -= key;
        }
    }

public:
    bool openFile(const std::string& filename, bool readOnly = false) {
        if (readOnly) {
            file.open(filename, std::ios::binary | std::ios::in);
        }
        else {
            file.open(filename, std::ios::binary | std::ios::in | std::ios::out);
        }
        return file.is_open();
    }

    bool extract(const std::string& outputPath) {
        if (!readHeader()) {
            std::cout << "Invalid TCD file" << std::endl;
            return false;
        }

        if (!readSections()) {
            std::cout << "No valid sections found" << std::endl;
            return false;
        }

        // 处理每个section
        for (size_t sectionIndex = 0; sectionIndex < sections.size(); sectionIndex++) {
            const auto& section = sections[sectionIndex];

            // 读取目录名
            file.seekg(section.indexOffset);
            std::vector<uint8_t> dirNames(section.dirCount * section.dirNameLength);
            file.read(reinterpret_cast<char*>(dirNames.data()), dirNames.size());
            uint8_t sectionKey = dirNames[section.dirNameLength - 1];
            decryptNames(dirNames, sectionKey);

            // 读取目录项
            std::vector<TcdDirEntry> dirs(section.dirCount);
            file.read(reinterpret_cast<char*>(dirs.data()), dirs.size() * sizeof(TcdDirEntry));

            // 读取文件名
            std::vector<uint8_t> fileNames(section.fileCount * section.fileNameLength);
            file.read(reinterpret_cast<char*>(fileNames.data()), fileNames.size());
            decryptNames(fileNames, sectionKey);

            // 读取文件偏移
            std::vector<uint32_t> offsets(section.fileCount + 1);
            file.read(reinterpret_cast<char*>(offsets.data()), offsets.size() * sizeof(uint32_t));

            // 提取文件
            int dirNameOffset = 0;
            for (const auto& dir : dirs) {
                std::string dirName = getName(dirNames, section.dirNameLength, dirNameOffset);
                int nameOffset = dir.namesOffset;

                for (int i = 0; i < dir.fileCount; i++) {
                    std::string fileName = getName(fileNames, section.fileNameLength, nameOffset);
                    std::string fullPath = outputPath + "/" + dirName;
                    fs::create_directories(fullPath);

                    std::string outFileName = fullPath + "/" + fileName + extensions[sectionIndex];
                    std::ofstream outFile(outFileName, std::ios::binary);

                    if (outFile) {
                        int index = dir.firstIndex + i;
                        uint32_t size = offsets[index + 1] - offsets[index];
                        std::vector<char> buffer(size);

                        file.seekg(offsets[index]);
                        file.read(buffer.data(), size);
                        outFile.write(buffer.data(), size);

                        std::cout << "Extracted: " << outFileName << std::endl;
                    }
                }
            }
        }

        return true;
    }

    bool updateFiles(const std::string& inputFile, const std::string& outputFile, const std::string& updatePath, int updateCount) {
        // 打开输入文件
        if (!openFile(inputFile, true)) {
            std::cout << "Failed to open input file: " << inputFile << std::endl;
            return false;
        }

        // 创建输出文件
        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            std::cout << "Failed to create output file: " << outputFile << std::endl;
            return false;
        }

        // 完整复制原文件
        file.seekg(0, std::ios::end);
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(fileSize);
        if (!file.read(buffer.data(), fileSize)) {
            std::cout << "Failed to read input file" << std::endl;
            return false;
        }

        outFile.write(buffer.data(), fileSize);

        // 读取头部和段信息
        if (!readHeader() || !readSections()) {
            std::cout << "Invalid TCD file or no valid sections found" << std::endl;
            return false;
        }

        int updatedFileCount = 0;

        // 更新文件
        for (size_t sectionIndex = 0; sectionIndex < sections.size() && updatedFileCount < updateCount; sectionIndex++) {
            auto& section = sections[sectionIndex];
            file.seekg(section.indexOffset);

            std::vector<uint8_t> dirNames(section.dirCount * section.dirNameLength);
            file.read(reinterpret_cast<char*>(dirNames.data()), dirNames.size());
            uint8_t sectionKey = dirNames[section.dirNameLength - 1];
            decryptNames(dirNames, sectionKey);

            std::vector<TcdDirEntry> dirs(section.dirCount);
            file.read(reinterpret_cast<char*>(dirs.data()), dirs.size() * sizeof(TcdDirEntry));

            std::vector<uint8_t> fileNames(section.fileCount * section.fileNameLength);
            file.read(reinterpret_cast<char*>(fileNames.data()), fileNames.size());
            decryptNames(fileNames, sectionKey);

            std::vector<uint32_t> offsets(section.fileCount + 1);
            file.read(reinterpret_cast<char*>(offsets.data()), offsets.size() * sizeof(uint32_t));

            int dirNameOffset = 0;
            for (const auto& dir : dirs) {
                std::string dirName = getName(dirNames, section.dirNameLength, dirNameOffset);
                int nameOffset = dir.namesOffset;

                for (int i = 0; i < dir.fileCount && updatedFileCount < updateCount; i++) {
                    std::string fileName = getName(fileNames, section.fileNameLength, nameOffset);
                    std::string fullPath = updatePath + "/" + dirName + "/" + fileName + extensions[sectionIndex];

                    if (fs::exists(fullPath)) {
                        // 更新文件
                        uint32_t newSize = fs::file_size(fullPath);
                        std::ifstream updateFile(fullPath, std::ios::binary);
                        std::vector<char> updateBuffer(newSize);
                        updateFile.read(updateBuffer.data(), newSize);

                        outFile.seekp(0, std::ios::end);
                        uint32_t newOffset = outFile.tellp();
                        outFile.write(updateBuffer.data(), newSize);

                        // 更新偏移信息
                        offsets[dir.firstIndex + i] = newOffset;
                        offsets[dir.firstIndex + i + 1] = newOffset + newSize;

                        updatedFileCount++;
                        std::cout << "Updated file: " << fullPath << std::endl;
                    }
                }
            }

            // 写入更新后的偏移信息
            uint32_t offsetPosition = section.indexOffset + section.dirCount * section.dirNameLength +
                section.dirCount * sizeof(TcdDirEntry) + section.fileCount * section.fileNameLength;
            outFile.seekp(offsetPosition);
            outFile.write(reinterpret_cast<char*>(offsets.data()), offsets.size() * sizeof(uint32_t));
        }

        std::cout << "Total updated files: " << updatedFileCount << std::endl;
        return true;
    }

    ~TcdManager() {
        if (file.is_open()) {
            file.close();
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Made by julixian 2025.01.01" << std::endl;
        std::cout << "Usage:" << std::endl;
        std::cout << "Extract: " << argv[0] << " extract <input.tcd> <output_dir>" << std::endl;
        std::cout << "Update:  " << argv[0] << " update <input.tcd> <output.tcd> <update_dir> <update_files_count>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    TcdManager manager;

    if (mode == "extract") {
        if (argc != 4) {
            std::cout << "Invalid number of arguments for extract mode" << std::endl;
            return 1;
        }
        if (!manager.openFile(argv[2], true)) {
            std::cout << "Failed to open file: " << argv[2] << std::endl;
            return 1;
        }
        if (!manager.extract(argv[3])) {
            std::cout << "Failed to extract files" << std::endl;
            return 1;
        }
        std::cout << "Extraction completed successfully" << std::endl;
    }
    else if (mode == "update") {
        if (argc != 6) {
            std::cout << "Invalid number of arguments for update mode" << std::endl;
            return 1;
        }
        int updateCount = std::atoi(argv[5]);
        if (updateCount <= 0) {
            std::cout << "Invalid update count. Please provide a positive integer." << std::endl;
            return 1;
        }
        if (!manager.updateFiles(argv[2], argv[3], argv[4], updateCount)) {
            std::cout << "Failed to update files" << std::endl;
            return 1;
        }
        std::cout << "Update completed successfully" << std::endl;
    }
    else {
        std::cout << "Invalid mode. Use 'extract' or 'update'." << std::endl;
        return 1;
    }

    return 0;
}
