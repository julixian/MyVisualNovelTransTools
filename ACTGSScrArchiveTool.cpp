#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

// 文件入口结构
struct Entry {
    uint32_t offset;
    uint32_t size;
    char name[0x18];
};

// 文件信息结构
struct FileInfo {
    std::string name;
    uint32_t size;
    std::vector<uint8_t> data;
};

// 加密/解密函数（异或操作）
void Crypt(uint8_t* data, int index, int length, const uint8_t* key, size_t keyLength) {
    for (int i = 0; i < length; ++i) {
        data[index + i] ^= key[i % keyLength];
    }
}

// 将字符串转换为字节数组
std::vector<uint8_t> ParseKey(const std::string& keyStr) {
    std::vector<uint8_t> key;
    std::string hexStr;

    if (keyStr.substr(0, 2) == "0x") {
        // 如果以0x开头，直接使用后面的十六进制字符串
        hexStr = keyStr.substr(2);
    }
    else {
        // 如果是十进制数，转换为十六进制字符串
        try {
            // 将十进制字符串转换为数值
            unsigned long long decNum = std::stoull(keyStr);

            // 转换为十六进制字符串
            std::stringstream ss;
            ss << std::hex << decNum;
            hexStr = ss.str();

            // 如果长度是奇数，前面补0
            if (hexStr.length() % 2 != 0) {
                hexStr = "0" + hexStr;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Invalid number format\n";
            return key;
        }
    }

    // 将十六进制字符串转换为字节数组
    for (size_t i = 0; i < hexStr.length(); i += 2) {
        std::string byteStr = hexStr.substr(i, 2);
        try {
            key.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
        }
        catch (const std::exception& e) {
            std::cerr << "Invalid hex value\n";
            key.clear();
            return key;
        }
    }

    return key;
}

// 解包函数
bool ExtractArchive(const std::string& inputFile, const std::string& outputDir, const std::vector<uint8_t>& key) {
    try {
        // 创建输出目录
        fs::create_directories(outputDir);

        // 打开输入文件
        std::ifstream file(inputFile, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open input file\n";
            return false;
        }

        // 读取文件数量
        uint32_t count;
        file.read(reinterpret_cast<char*>(&count), 4);

        // 验证保留字段
        uint32_t reserved;
        for (int i = 0; i < 3; ++i) {
            file.read(reinterpret_cast<char*>(&reserved), 4);
            if (reserved != 0) {
                std::cerr << "Invalid file format\n";
                return false;
            }
        }

        // 读取索引
        std::vector<Entry> entries(count);
        std::vector<uint8_t> indexData(count * sizeof(Entry));

        file.read(reinterpret_cast<char*>(indexData.data()), indexData.size());
        Crypt(indexData.data(), 0, indexData.size(), key.data(), key.size());

        for (uint32_t i = 0; i < count; ++i) {
            memcpy(&entries[i], &indexData[i * sizeof(Entry)], sizeof(Entry));
        }

        // 提取文件
        for (const auto& entry : entries) {
            std::string outPath = outputDir + "/" + entry.name;
            std::cout << "Extracting: " << entry.name << std::endl;

            std::vector<uint8_t> fileData(entry.size);
            file.seekg(entry.offset);
            file.read(reinterpret_cast<char*>(fileData.data()), entry.size);

            if (fileData[0] == 'X') {
                Crypt(fileData.data(), 1, entry.size - 1, key.data(), key.size());
                fileData[0] = 'N';
            }

            std::ofstream outFile(outPath, std::ios::binary);
            if (!outFile) {
                std::cerr << "Failed to create output file: " << entry.name << std::endl;
                continue;
            }
            outFile.write(reinterpret_cast<char*>(fileData.data()), entry.size);
        }

        std::cout << "Extraction completed successfully!\n";
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error during extraction: " << e.what() << std::endl;
        return false;
    }
}

// 封包函数
bool CreateArchive(const std::string& inputDir, const std::string& outputFile, const std::vector<uint8_t>& key) {
    try {
        // 收集所有.scr文件
        std::vector<FileInfo> files;
        for (const auto& entry : fs::directory_iterator(inputDir)) {
            if (entry.path().extension() == ".scr") {
                FileInfo fileInfo;
                fileInfo.name = entry.path().filename().string();
                fileInfo.size = fs::file_size(entry.path());

                std::ifstream inFile(entry.path(), std::ios::binary);
                fileInfo.data.resize(fileInfo.size);
                inFile.read(reinterpret_cast<char*>(fileInfo.data.data()), fileInfo.size);

                if (fileInfo.data[0] == 'N') {
                    fileInfo.data[0] = 'X';
                    Crypt(fileInfo.data.data(), 1, fileInfo.size - 1, key.data(), key.size());
                }

                files.push_back(std::move(fileInfo));
            }
        }

        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create output file\n";
            return false;
        }

        uint32_t fileCount = static_cast<uint32_t>(files.size());
        uint32_t zero = 0;

        outFile.write(reinterpret_cast<char*>(&fileCount), 4);
        for (int i = 0; i < 3; ++i) {
            outFile.write(reinterpret_cast<char*>(&zero), 4);
        }

        const uint32_t headerSize = 16;
        const uint32_t indexSize = sizeof(Entry) * fileCount;
        uint32_t currentOffset = headerSize + indexSize;

        std::vector<Entry> entries(fileCount);
        for (size_t i = 0; i < files.size(); ++i) {
            Entry& entry = entries[i];
            entry.offset = currentOffset;
            entry.size = files[i].size;
            strncpy(entry.name, files[i].name.c_str(), sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';

            currentOffset += entry.size;
        }

        std::vector<uint8_t> indexData(indexSize);
        memcpy(indexData.data(), entries.data(), indexSize);
        Crypt(indexData.data(), 0, indexSize, key.data(), key.size());

        outFile.write(reinterpret_cast<char*>(indexData.data()), indexSize);

        for (const auto& file : files) {
            outFile.write(reinterpret_cast<const char*>(file.data.data()), file.size);
        }

        std::cout << "Successfully created archive with " << fileCount << " files.\n";
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error during packing: " << e.what() << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.02.15" << std::endl;
        std::cout << "Usage:\n";
        std::cout << "For unpacking: " << argv[0] << " -u <input.scr> <output_directory>\n";
        std::cout << "For packing: " << argv[0] << " -p <input_directory> <output.scr>\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string input = argv[2];
    std::string output = argv[3];

    std::string keyStr;
    std::cout << "Enter key (support 0x for hex): ";
    std::getline(std::cin, keyStr);

    std::vector<uint8_t> key = ParseKey(keyStr);
    if (key.empty()) {
        std::cerr << "Invalid key format\n";
        return 1;
    }

    if (mode == "-u") {
        return ExtractArchive(input, output, key) ? 0 : 1;
    }
    else if (mode == "-p") {
        return CreateArchive(input, output, key) ? 0 : 1;
    }
    else {
        std::cerr << "Invalid mode. Use -u for unpacking or -p for packing.\n";
        return 1;
    }
}
