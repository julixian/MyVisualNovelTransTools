#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <Windows.h>
#include <string.h>

namespace fs = std::filesystem;

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}
std::wstring AsciiToWide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}
std::string AsciiToAscii(const std::string& ascii, UINT src, UINT dst) {
    return WideToAscii(AsciiToWide(ascii, src), dst);
}

struct FileEntry {
    std::wstring fileName;
    uint32_t offset;
    uint32_t fileSize;
};

bool extractPackage(const std::string& packagePath, const std::string& outputDir) {

    std::ifstream packageFile(packagePath, std::ios::binary);
    if (!packageFile) {
        std::cerr << "Error: Could not open package file: " << AsciiToAscii(packagePath, CP_ACP, 65001) << std::endl;
        return false;
    }
    char header[3];
    packageFile.read(header, 3);
    if (std::strncmp(header, "PAC", 3) != 0) {
        std::cerr << "Error: Invalid package format. Expected 'PAC' header." << std::endl;
        return false;
    }
    uint32_t dataBase;
    packageFile.read(reinterpret_cast<char*>(&dataBase), 4);

    std::vector<FileEntry> entries;
    std::vector<uint8_t> indexData(dataBase - 0x7);
    packageFile.read((char*)indexData.data(), dataBase - 0x7);
    for (size_t i = 0; i < indexData.size();) {
        FileEntry entry;
        std::string fileName((char*)&indexData[i]);
        for (auto& ch : fileName) {
            ch ^= 0xFF;
        }
        entry.fileName = AsciiToWide(fileName, 932);
        i += fileName.length();
        i++;
        entry.offset = *(uint32_t*)&indexData[i];
        i += 4;
        entry.fileSize = *(uint32_t*)&indexData[i];
        i += 4;
        entries.push_back(entry);
        std::cout << "Processing: " << AsciiToAscii(fileName, 932, 65001) << "\n"
            << "offset: 0x" << std::hex << entry.offset << "\n"
            << "fileSize: 0x" << std::hex << entry.fileSize << std::endl;
    }

    for (const auto& entry : entries) {

        std::vector<uint8_t> fileData(entry.fileSize);
        packageFile.seekg(entry.offset + dataBase);
        packageFile.read(reinterpret_cast<char*>(fileData.data()), entry.fileSize);
        for (size_t j = 0; j < 3; j++) {
            fileData[j] ^= 0xFF;
        }

        std::wstring fullPath = AsciiToWide(outputDir, CP_ACP) + L"\\" + entry.fileName;
        std::filesystem::path filePath(fullPath);
        std::filesystem::create_directories(filePath.parent_path());
        std::ofstream outFile(fullPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error: Could not create output file: " << WideToAscii(fullPath, 65001) << std::endl;
            continue;
        }
        outFile.write(reinterpret_cast<char*>(fileData.data()), fileData.size());
        outFile.close();
        std::cout << "  Successfully extracted to: " << WideToAscii(fullPath, 65001) << std::endl;
    }
    packageFile.close();
    return true;
}

bool createPackage(const std::string& inputDir, const std::string& packagePath) {

    if (!std::filesystem::exists(inputDir) || !std::filesystem::is_directory(inputDir)) {
        std::cerr << "Error: Input directory does not exist: " << AsciiToAscii(inputDir, CP_ACP, 65001) << std::endl;
        return false;
    }
    std::vector<std::filesystem::path> files;
    std::filesystem::path inputPath(inputDir);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) {
        std::cerr << "Error: No files found in the input directory." << std::endl;
        return false;
    }
    std::cout << "Found " << files.size() << " files to package." << std::endl;
    std::ofstream packageFile(packagePath, std::ios::binary);
    if (!packageFile) {
        std::cerr << "Error: Could not create package file: " << AsciiToAscii(packagePath, CP_ACP, 65001) << std::endl;
        return false;
    }

    packageFile.write("PAC", 3);
    packageFile.seekp(0x7);
    uint32_t currentOffset = 0;
    for (fs::path& file : files) {
        std::string fileName = WideToAscii(fs::relative(file, inputDir).wstring(), 932);
        for (auto& ch : fileName) {
            ch ^= 0xFF;
        }
        uint32_t fileSize = fs::file_size(file);
        packageFile.write(fileName.c_str(), fileName.length() + 1);
        packageFile.write((char*)&currentOffset, 4);
        packageFile.write((char*)&fileSize, 4);
        currentOffset += fileSize;
    }

    uint32_t dataBase = packageFile.tellp();
    packageFile.seekp(0x3);
    packageFile.write((char*)&dataBase, 4);
    packageFile.seekp(dataBase);

    for (fs::path& file : files) {
        std::ifstream ifs(file, std::ios::binary);
        if (!ifs) {
            std::cout << "Fail to open: " << (char*)file.u8string().c_str() << std::endl;
            return false;
        }
        std::vector<uint8_t> fileData(fs::file_size(file));
        ifs.read((char*)fileData.data(), fileData.size());
        ifs.close();
        for (size_t j = 0; j < 3 && j < fileData.size(); j++) {
            fileData[j] ^= 0xFF;
        }
        packageFile.write((char*)fileData.data(), fileData.size());
    }

    packageFile.close();
    std::cout << "Package created successfully: " << AsciiToAscii(packagePath, CP_ACP, 65001) << std::endl;
    return true;
}

void printUsage(const char* programName) {
    std::cout << "Made by julixian 2025.05.22" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "For extract: " << AsciiToAscii(programName, CP_ACP, 65001) << " -e <package_file> <output_directory>" << std::endl;
    std::cout << "For pack:  " << AsciiToAscii(programName, CP_ACP, 65001) << " -p <input_directory> <package_file>" << std::endl;
}

int main(int argc, char* argv[]) {
    system("chcp 65001");
    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }
    std::string mode = argv[1];
    std::string path1 = argv[2];
    std::string path2 = argv[3];
    if (mode == "-e") {
        if (extractPackage(path1, path2)) {
            std::cout << "Extraction completed successfully!" << std::endl;
            return 0;
        }
        else {
            std::cerr << "Extraction failed!" << std::endl;
            return 1;
        }
    }
    else if (mode == "-p") {
        if (createPackage(path1, path2)) {
            std::cout << "Package created successfully!" << std::endl;
            return 0;
        }
        else {
            std::cerr << "Package creation failed!" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Error: Unknown mode. Use -e for extract or -p for create." << std::endl;
        printUsage(argv[0]);
        return 1;
    }
}