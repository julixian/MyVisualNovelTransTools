#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

struct DatHeader {
    uint32_t index_entries;
    uint32_t reserved[3] = { 0, 0, 0 };
};

struct DatEntry {
    uint32_t offset;
    uint32_t length;
    char name[24];
};

// 加密解密用同一个函数即可,因为是异或操作
void crypt(uint8_t* data, size_t len, const std::vector<uint8_t>& key) {
    for (size_t i = 0; i < len; ++i) {
        data[i] ^= key[i % key.size()];
    }
}

std::vector<uint8_t> hexStringToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    size_t start = (hex.substr(0, 2) == "0x") ? 2 : 0;

    for (size_t i = start; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t)strtol(byteString.c_str(), nullptr, 16);
        bytes.push_back(byte);
    }

    return bytes;
}

bool unpackScr(const std::string& inputFile, const std::string& outputDir, const std::vector<uint8_t>& key) {
    std::ifstream fin(inputFile, std::ios::binary);
    if (!fin) {
        std::cout << "Error: Cannot open input file!" << std::endl;
        return false;
    }

    DatHeader header;
    fin.read(reinterpret_cast<char*>(&header), sizeof(header));
    crypt(reinterpret_cast<uint8_t*>(&header), sizeof(header), key);

    if (header.index_entries <= 0) {
        std::cout << "Error: Invalid header!" << std::endl;
        return false;
    }

    std::vector<DatEntry> entries(header.index_entries);
    fin.read(reinterpret_cast<char*>(entries.data()), sizeof(DatEntry) * header.index_entries);
    crypt(reinterpret_cast<uint8_t*>(entries.data()), sizeof(DatEntry) * header.index_entries, key);

    if (entries[0].offset != (sizeof(header) + header.index_entries * sizeof(DatEntry))) {
        std::cout << "Error: Index table decryption failed!" << std::endl;
        return false;
    }

    fs::create_directories(outputDir);

    for (const auto& entry : entries) {
        std::string outPath = outputDir + "/" + entry.name;

        std::vector<uint8_t> fileData(entry.length);
        fin.seekg(entry.offset);
        fin.read(reinterpret_cast<char*>(fileData.data()), entry.length);

        if (fileData[0] == 'X') {
            fileData[0] = 'N';
            crypt(fileData.data() + 1, entry.length - 1, key);
        }

        std::ofstream fout(outPath, std::ios::binary);
        if (!fout) {
            std::cout << "Error: Cannot create output file: " << entry.name << std::endl;
            continue;
        }

        fout.write(reinterpret_cast<char*>(fileData.data()), entry.length);
        fout.close();

        std::cout << "Unpacked: " << entry.name << std::endl;
    }

    fin.close();
    return true;
}

bool packScr(const std::string& inputDir, const std::string& outputFile, const std::vector<uint8_t>& key) {
    std::vector<fs::path> scrFiles;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.path().extension() == ".scr") {
            scrFiles.push_back(entry.path());
        }
    }

    if (scrFiles.empty()) {
        std::cout << "Error: No .scr files found in input directory!" << std::endl;
        return false;
    }

    std::sort(scrFiles.begin(), scrFiles.end());

    DatHeader header;
    header.index_entries = static_cast<uint32_t>(scrFiles.size());

    uint32_t currentOffset = sizeof(DatHeader) + header.index_entries * sizeof(DatEntry);

    std::vector<DatEntry> entries(header.index_entries);
    std::vector<std::vector<uint8_t>> fileContents(header.index_entries);

    for (size_t i = 0; i < scrFiles.size(); ++i) {
        std::ifstream fin(scrFiles[i], std::ios::binary);
        if (!fin) {
            std::cout << "Error: Cannot open input file: " << scrFiles[i] << std::endl;
            return false;
        }

        fin.seekg(0, std::ios::end);
        size_t fileSize = fin.tellg();
        fin.seekg(0);

        fileContents[i].resize(fileSize);
        fin.read(reinterpret_cast<char*>(fileContents[i].data()), fileSize);
        fin.close();

        if (fileContents[i][0] == 'N') {
            fileContents[i][0] = 'X';
            crypt(fileContents[i].data() + 1, fileSize - 1, key);
        }

        DatEntry& entry = entries[i];
        entry.offset = currentOffset;
        entry.length = static_cast<uint32_t>(fileSize);

        std::string filename = scrFiles[i].filename().string();
        std::memset(entry.name, 0, 24);
        std::strncpy(entry.name, filename.c_str(), std::min(filename.length(), size_t(23)));

        currentOffset += fileSize;
    }

    std::ofstream fout(outputFile, std::ios::binary);
    if (!fout) {
        std::cout << "Error: Cannot create output file!" << std::endl;
        return false;
    }

    DatHeader encryptedHeader = header;
    crypt(reinterpret_cast<uint8_t*>(&encryptedHeader), sizeof(DatHeader), key);
    fout.write(reinterpret_cast<char*>(&encryptedHeader), sizeof(DatHeader));

    std::vector<DatEntry> encryptedEntries = entries;
    crypt(reinterpret_cast<uint8_t*>(encryptedEntries.data()),
        sizeof(DatEntry) * encryptedEntries.size(), key);
    fout.write(reinterpret_cast<char*>(encryptedEntries.data()),
        sizeof(DatEntry) * encryptedEntries.size());

    for (const auto& content : fileContents) {
        fout.write(reinterpret_cast<const char*>(content.data()), content.size());
    }

    fout.close();
    return true;
}

void printUsage() {
    std::cout << "Made by julixian 2025.02.27" << std::endl;
    std::cout << "Usage: ./programm.exe <unpack/pack> <input> <output> <key>" << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  scr_tool unpack data.scr ./output 0x10D3275310D32753C608331251881921A1" << std::endl;
    std::cout << "  scr_tool pack ./input output.scr 0x10D3275310D32753C608331251881921A1" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];
    std::string input = argv[2];
    std::string output = argv[3];
    std::string keyHex = argv[4];

    std::vector<uint8_t> key = hexStringToBytes(keyHex);
    if (key.empty()) {
        std::cout << "Error: Invalid key format!" << std::endl;
        return 1;
    }

    bool success;
    if (command == "unpack") {
        success = unpackScr(input, output, key);
    }
    else if (command == "pack") {
        success = packScr(input, output, key);
    }
    else {
        std::cout << "Error: Unknown command!" << std::endl;
        printUsage();
        return 1;
    }

    if (success) {
        std::cout << "Operation completed successfully!" << std::endl;
    }
    else {
        std::cout << "Operation failed!" << std::endl;
        return 1;
    }

    return 0;
}
