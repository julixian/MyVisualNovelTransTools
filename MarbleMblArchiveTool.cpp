#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

struct FileEntry16 {
    char name[16];
    uint32_t offset;
    uint32_t size;
};

struct FileEntry56 {
    char name[56];
    uint32_t offset;
    uint32_t size;
};

std::vector<uint8_t> parseKey(const std::string& keyStr) {
    std::vector<uint8_t> key;
    if (keyStr.substr(0, 2) == "0x") {
        for (size_t i = 2; i < keyStr.length(); i += 2) {
            uint8_t byte = std::stoi(keyStr.substr(i, 2), nullptr, 16);
            key.push_back(byte);
        }
    }
    else {
        uint32_t decimalKey = std::stoul(keyStr);
        key.resize(4);
        for (int i = 0; i < 4; ++i) {
            key[i] = (decimalKey >> (i * 8)) & 0xFF;
        }
    }
    return key;
}

std::string processFileName(const char* name, size_t maxLen) {
    std::string fileName;
    size_t nameLen = 0;

    while (nameLen < maxLen && name[nameLen] != '\0') {
        nameLen++;
    }

    fileName = std::string(name, nameLen);

    if (nameLen + 1 < maxLen) {
        size_t extStart = nameLen + 1;
        size_t extLen = 0;
        while (extStart + extLen < maxLen && name[extStart + extLen] != '\0') {
            extLen++;
        }

        if (extLen > 0) {
            fileName += "." + std::string(name + extStart, extLen);
        }
    }

    return fileName;
}

void processFileNamePack(const std::string& fullName, char* output, size_t maxLen) {
    std::string name = fs::path(fullName).stem().string();
    std::string ext = fs::path(fullName).extension().string();

    if (ext.length() > 0 && ext[0] == '.') {
        ext = ext.substr(1);
    }

    memset(output, 0, maxLen);
    strncpy(output, name.c_str(), std::min(name.length(), maxLen - 1));

    if (!ext.empty()) {
        size_t nameLen = strlen(output);
        if (nameLen < maxLen - 1) {
            output[nameLen] = '\0';
            strncpy(output + nameLen + 1, ext.c_str(), maxLen - nameLen - 2);
        }
    }
}

template<typename T>
void extractMBL(const std::string& mblPath, const std::string& outputDir, const std::vector<uint8_t>& key) {
    std::ifstream mblFile(mblPath, std::ios::binary);
    if (!mblFile) {
        std::cerr << "Failed to open MBL file: " << mblPath << std::endl;
        return;
    }

    std::string listFilePath = fs::path(outputDir).parent_path().string() + "/file_list.txt";
    std::ofstream listFile(listFilePath);
    if (!listFile) {
        std::cerr << "Failed to create file list: " << listFilePath << std::endl;
        return;
    }

    uint32_t fileCount;
    mblFile.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));

    std::vector<T> entries(fileCount);
    mblFile.read(reinterpret_cast<char*>(entries.data()), fileCount * sizeof(T));

    fs::create_directories(outputDir);

    for (const auto& entry : entries) {
        std::string fileName = processFileName(entry.name, sizeof(entry.name));
        std::string filePath = outputDir + "/" + fileName;

        listFile << fileName << std::endl;

        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create output file: " << filePath << std::endl;
            continue;
        }

        mblFile.seekg(entry.offset);

        std::vector<char> buffer(entry.size);
        mblFile.read(buffer.data(), buffer.size());

        if (!key.empty()) {
            for (size_t i = 0; i < buffer.size(); ++i) {
                buffer[i] ^= key[i % key.size()];
            }
        }
        else {
            for (char& byte : buffer) {
                byte = ~byte + 1;
            }
        }

        outFile.write(buffer.data(), buffer.size());
        outFile.close();

        std::cout << "Extracted: " << fileName << std::endl;
    }

    mblFile.close();
    listFile.close();
    std::cout << "Extraction complete. File list saved to: " << listFilePath << std::endl;
}

template<typename T>
void createMBL(const std::string& inputDir, const std::string& outputPath, const std::vector<uint8_t>& key) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (fs::is_regular_file(entry)) {
            files.push_back(entry.path());
        }
    }

    std::ofstream mblFile(outputPath, std::ios::binary);
    if (!mblFile) {
        std::cerr << "Failed to create MBL file: " << outputPath << std::endl;
        return;
    }

    uint32_t fileCount = files.size();
    mblFile.write(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));

    uint32_t currentOffset = 4 + fileCount * sizeof(T);

    std::vector<T> entries(fileCount);
    for (size_t i = 0; i < fileCount; ++i) {
        processFileNamePack(files[i].filename().string(), entries[i].name, sizeof(entries[i].name));
        entries[i].offset = currentOffset;
        entries[i].size = fs::file_size(files[i]);
        currentOffset += entries[i].size;
    }

    mblFile.write(reinterpret_cast<char*>(entries.data()), fileCount * sizeof(T));

    for (const auto& file : files) {
        std::ifstream inputFile(file, std::ios::binary);
        std::vector<char> buffer(fs::file_size(file));
        inputFile.read(buffer.data(), buffer.size());

        if (!key.empty()) {
            for (size_t i = 0; i < buffer.size(); ++i) {
                buffer[i] ^= key[i % key.size()];
            }
        }
        else {
            for (char& byte : buffer) {
                byte = ~(byte - 1);
            }
        }

        mblFile.write(buffer.data(), buffer.size());
        std::cout << "Packed: " << file.filename().string() << std::endl;
    }

    mblFile.close();
    std::cout << "Packing complete. MBL file created: " << outputPath << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 5 || argc > 6) {
        std::cerr << "Made by julixian 2025.01.30" << std::endl;
        std::cerr << "Usage: " << std::endl;
        std::cerr << "For extraction: " << argv[0] << " extract <mbl_file_path> <output_directory> <IndexLength> [key]" << std::endl;
        std::cerr << "For packing: " << argv[0] << " pack <input_directory> <output_mbl_file> <IndexLength> [key]" << std::endl;
        std::cerr << "Key can be empty, decimal or hexadecimal (prefixed with 0x)" << std::endl;
        std::cerr << "Index length can be 24 or 64" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string path1 = argv[2];
    std::string path2 = argv[3];
    int indexLength = std::stoi(argv[4]);
    std::vector<uint8_t> key;

    if (argc == 6) {
        key = parseKey(argv[5]);
    }

    if (mode == "extract") {
        if (indexLength == 24) {
            extractMBL<FileEntry16>(path1, path2, key);
        }
        else if (indexLength == 64) {
            extractMBL<FileEntry56>(path1, path2, key);
        }
        else {
            std::cerr << "Invalid name length. Use 24 or 64." << std::endl;
            return 1;
        }
    }
    else if (mode == "pack") {
        if (indexLength == 24) {
            createMBL<FileEntry16>(path1, path2, key);
        }
        else if (indexLength == 64) {
            createMBL<FileEntry56>(path1, path2, key);
        }
        else {
            std::cerr << "Invalid name length. Use 24 or 64." << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Invalid mode. Use 'extract' or 'pack'." << std::endl;
        return 1;
    }

    return 0;
}
