#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <format>
#include <chrono>
#include <Windows.h>

namespace fs = std::filesystem;

extern "C" size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);
extern "C" size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);

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

#pragma pack(1)
struct FileEntry {
    char filename[0x40] = {};       // 文件名
    uint32_t fileSize;         // 文件大小
    uint32_t offset;           // 偏移量
    uint32_t compressionFlag;  // 压缩标志 (1=压缩, 0=未压缩)
    uint32_t decompressedSize; // 解压后大小
    char filetime[0x14] = {};
};
#pragma pack()

std::string getFormattedFileTime(const fs::path& filePath) {
    auto fileTime = fs::last_write_time(filePath);
    return std::format("{:%Y/%m/%d %H:%M:%S}", fileTime);
}

void encryptIndex(uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        uint8_t swapped = ((data[i] & 0x0F) << 4) | ((data[i] & 0xF0) >> 4);
        data[i] = swapped ^ 0xFF;
    }
}
void decryptIndex(uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        uint8_t xored = data[i] ^ 0xFF;
        data[i] = ((xored & 0x0F) << 4) | ((xored & 0xF0) >> 4);
    }
}

bool extractPackage(const std::string& packagePath, const std::string& outputDir) {
    std::ifstream file(packagePath, std::ios::binary);
    if (!file) {
        std::cerr << "Can not open: " << packagePath << std::endl;
        return false;
    }

    std::filesystem::create_directories(outputDir);

    uint32_t fileCount = 0;
    uint32_t headerSize = 0;
    uint32_t dataOffset = 0;

    file.read(reinterpret_cast<char*>(&fileCount), 4);
    file.read(reinterpret_cast<char*>(&headerSize), 4);
    file.read(reinterpret_cast<char*>(&dataOffset), 4);

    std::cout << "fileCount: " << fileCount << std::endl;
    std::cout << "headerSize: " << headerSize << std::endl;
    std::cout << "dataOffset: " << dataOffset << std::endl;

    if (headerSize != 0xC) {
        std::cerr << "Error: Not an expected header size (0xC)" << std::endl;
        return false;
    }

    size_t indexSize = dataOffset - headerSize;
    if (indexSize % 0x64 != 0 || indexSize / 0x64 != fileCount) {
        std::cerr << "Error: Not an expected index size" << std::endl;
        return false;
    }

    std::vector<uint8_t> indexData(indexSize);
    file.read(reinterpret_cast<char*>(indexData.data()), indexSize);

    decryptIndex(indexData.data(), indexSize);

    std::vector<FileEntry> fileEntries;
    for (uint32_t i = 0; i < fileCount; i++) {
        FileEntry entry;
        uint8_t* entryPtr = indexData.data() + i * 0x64;

        memcpy(entry.filename, entryPtr, 0x40);

        entry.fileSize = *reinterpret_cast<uint32_t*>(entryPtr + 0x40);
        entry.offset = *reinterpret_cast<uint32_t*>(entryPtr + 0x44);
        entry.compressionFlag = *reinterpret_cast<uint32_t*>(entryPtr + 0x48);
        entry.decompressedSize = *reinterpret_cast<uint32_t*>(entryPtr + 0x4C);
        memcpy(entry.filetime, entryPtr + 0x50, 0x14);
        fileEntries.push_back(entry);
    }

    for (const auto& entry : fileEntries) {
        std::string filename = entry.filename;
        if (filename.empty()) continue;

        std::cout << "Extracting: " << AsciiToAscii(filename, 932, CP_ACP) << " (Size: " << entry.fileSize
            << ", Offset: " << entry.offset
            << ", Compressed: " << (entry.compressionFlag ? "Yes" : "No")
            << ", Decompr length: " << entry.decompressedSize << ")" << "\n"
            << "File time: " << entry.filetime << std::endl;

        std::string tmpPath = outputDir + "\\" + filename;
        std::wstring outputPath = AsciiToWide(tmpPath, 932);

        std::filesystem::path filePath(outputPath);
        std::filesystem::create_directories(filePath.parent_path());

        std::vector<uint8_t> compressedData(entry.fileSize);
        file.seekg(dataOffset + entry.offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(compressedData.data()), entry.fileSize);

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Can not create: " << WideToAscii(outputPath, CP_ACP) << std::endl;
            continue;
        }

        if (entry.compressionFlag) {
            // 需要解压
            std::vector<uint8_t> decompressedData(entry.decompressedSize);
            size_t actualSize = lzss_decompress(
                decompressedData.data(), entry.decompressedSize,
                compressedData.data(), entry.fileSize
            );

            if (actualSize != entry.decompressedSize) {
                std::cerr << "Warning: Not an expected decompr length (expected: " << entry.decompressedSize
                    << ", actual: " << actualSize << ")" << std::endl;
            }

            outFile.write(reinterpret_cast<char*>(decompressedData.data()), actualSize);
        }
        else {
            outFile.write(reinterpret_cast<char*>(compressedData.data()), entry.fileSize);
        }

        outFile.close();
    }

    file.close();
    std::cout << "Extract successfully!" << std::endl;
    return true;
}

bool createPackage(const std::string& inputDir, const std::string& packagePath, bool useCompression = false) {

    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cerr << "Error: Not exist or not a directory: " << inputDir << std::endl;
        return false;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    if (files.empty()) {
        std::cerr << "Error: No file to pack" << std::endl;
        return false;
    }

    uint32_t fileCount = static_cast<uint32_t>(files.size());
    std::cout << "Find " << fileCount << " files to pack" << std::endl;

    uint32_t headerSize = 0xC; 
    size_t entrySize = sizeof(FileEntry);
    uint32_t dataOffset = headerSize + fileCount * entrySize;

    std::vector<FileEntry> fileEntries;
    std::vector<std::vector<uint8_t>> fileContents;
    uint32_t currentOffset = 0;

    for (const auto& filePath : files) {

        std::ifstream inFile(filePath, std::ios::binary | std::ios::ate);
        if (!inFile) {
            std::cerr << "Can not open: " << filePath << std::endl;
            continue;
        }

        std::streamsize originalSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        std::vector<uint8_t> fileData(originalSize);
        inFile.read(reinterpret_cast<char*>(fileData.data()), originalSize);
        inFile.close();

        FileEntry entry;
        fs::path relativePath = fs::relative(filePath, inputDir);
        std::string relativePathStr = WideToAscii(relativePath.wstring(), 932);

        if (relativePathStr.length() >= sizeof(entry.filename) - 1) {
            std::cerr << "Warning: filename is too long, which will be truncated: " << relativePathStr << std::endl;
            relativePathStr = relativePathStr.substr(0, sizeof(entry.filename) - 1);
        }
        strcpy_s(entry.filename, relativePathStr.c_str());

        std::string fileTime = getFormattedFileTime(filePath);
        if (fileTime.length() >= sizeof(entry.filetime) - 1) {
            fileTime = fileTime.substr(0, sizeof(entry.filetime) - 1);
        }
        strcpy_s(entry.filetime, fileTime.c_str());

        entry.offset = currentOffset;

        if (useCompression) {
            std::vector<uint8_t> compressedData((originalSize + 7) / 8 * 9);
            size_t actualLen = lzss_compress(compressedData.data(), compressedData.size(), fileData.data(), fileData.size());
            compressedData.resize(actualLen);
            entry.compressionFlag = 1;
            entry.fileSize = compressedData.size();
            entry.decompressedSize = originalSize;
            fileContents.push_back(compressedData);
        }
        else {
            entry.compressionFlag = 0;
            entry.fileSize = originalSize;
            entry.decompressedSize = originalSize;
            fileContents.push_back(fileData);
        }

        currentOffset += entry.fileSize;

        fileEntries.push_back(entry);

        std::cout << "Adding file: " << relativePath.string() << " (Size: " << entry.fileSize
            << ", Offset: " << entry.offset
            << ", Compress: " << (entry.compressionFlag ? "Yes" : "No") << ")" << "\n"
            << "File time: " << fileTime << std::endl;
    }

    std::ofstream outFile(packagePath, std::ios::binary);
    if (!outFile) {
        std::cerr << "Can not create " << packagePath << std::endl;
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(&fileCount), 4);
    outFile.write(reinterpret_cast<const char*>(&headerSize), 4);
    outFile.write(reinterpret_cast<const char*>(&dataOffset), 4);

    std::vector<uint8_t> indexData(fileCount * entrySize);
    for (size_t i = 0; i < fileEntries.size(); i++) {
        memcpy(indexData.data() + i * entrySize, &fileEntries[i], entrySize);
    }

    encryptIndex(indexData.data(), indexData.size());

    outFile.write(reinterpret_cast<const char*>(indexData.data()), indexData.size());

    for (const auto& fileData : fileContents) {
        outFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
    }

    outFile.close();
    std::cout << "Pack successfully! Created archive: " << packagePath << std::endl;
    return true;
}

void printUsage(const char* programName) {
    std::cout << "Made by julixian 2025.05.08" << "\n"
        << "Usage:\n"
        << "For extract: " << programName << " -e <data_file> <output_dir>\n"
        << "For pack: " << programName << " -p <input_dir> <output_file> [--lzss]\n"
        << "--lzss: " << "Use lzss compress when packing" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "-e") {
        std::string packagePath = argv[2];
        std::string outputDir = argv[3];

        if (!extractPackage(packagePath, outputDir)) {
            std::cerr << "Fail to extract!" << std::endl;
            return 1;
        }
    }
    else if (mode == "-p") {
        std::string inputDir = argv[2];
        std::string packagePath = argv[3];
        bool useCompression = false;

        if (argc > 4 && std::string(argv[4]) == "--lzss") {
            useCompression = true;
        }

        if (!createPackage(inputDir, packagePath, useCompression)) {
            std::cerr << "Fail to pack!" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Invalid mode: " << mode << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
