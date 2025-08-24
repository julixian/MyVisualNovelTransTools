#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;

void decryptStr(std::string& str) {
    for (uint32_t i = 0; i < str.length(); ++i) {
        uint32_t v4 = i + 1;
        uint8_t key = 9 - (v4 % 10) + 0x80;
        str[i] ^= key;
    }
}

void encryptStr(std::string& str) {
    decryptStr(str);
}

void extract(fs::path inputPath, fs::path outputPath) {
    std::ifstream ifs(inputPath, std::ios::binary);
    if (outputPath.has_parent_path()) {
        fs::create_directories(outputPath.parent_path());
    }
    std::ofstream ofs(outputPath);
    if (!ifs.is_open()) {
        throw std::runtime_error(std::format("Failed to open input file: {}", inputPath.string()));
    }
    if (!ofs.is_open()) {
        throw std::runtime_error(std::format("Failed to open output file: {}", outputPath.string()));
    }
    char magic[16];
    ifs.read(magic, 16);
    if (std::string(magic, 16) != "CromwellPresent.") {
        throw std::runtime_error("Invalid header.");
    }

    uint32_t strCount = 0;
    ifs.read((char*)&strCount, 4);
    std::vector<uint32_t> strOffsets(strCount);

    for (uint32_t i = 0; i < strCount; i++) {
        ifs.read((char*)&strOffsets[i], 4);
    }

    for (uint32_t i = 0; i < strOffsets.size(); i++) {
        ifs.seekg(strOffsets[i]);
        uint32_t strLength = 0;
        ifs.read((char*)&strLength, 4);
        
        std::string str(strLength, 0);
        ifs.read(str.data(), strLength);

        decryptStr(str);
        ofs << str << std::endl;
    }
}

void pack(fs::path inputPath, fs::path outputPath) {
    std::ifstream ifs(inputPath);
    if (outputPath.has_parent_path()) {
        fs::create_directories(outputPath.parent_path());
    }
    std::ofstream ofs(outputPath, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error(std::format("Failed to open input file: {}", inputPath.string()));
    }
    if (!ofs.is_open()) {
        throw std::runtime_error(std::format("Failed to open output file: {}", outputPath.string()));
    }
    ofs.write("CromwellPresent.", 16);

    std::vector<std::string> strs;
    std::string line;
    while (std::getline(ifs, line)) {
        strs.push_back(line);
    }

    uint32_t strCount = (uint32_t)strs.size();
    ofs.write((char*)&strCount, 4);

    uint32_t dataBase = (uint32_t)ofs.tellp() + 4 * strCount;
    uint32_t currentOffset = dataBase;

    for (uint32_t i = 0; i < strs.size(); i++) {
        ofs.write((char*)&currentOffset, 4);
        currentOffset += 4 + strs[i].length();
    }

    for (uint32_t i = 0; i < strs.size(); i++) {
        uint32_t strLength = (uint32_t)strs[i].length();
        ofs.write((char*)&strLength, 4);
        std::string& str = strs[i];
        encryptStr(str);
        ofs.write(str.data(), strLength);
    }
}

void printUsage(fs::path programPath) {
    std::print("Made by julixian 2025.08.24\n"
        "Usage:\n"
        "For extract: {0} extract <input.crp> <output.txt>\n"
        "For pack: {0} pack <input.txt> <output.crp>\n"
        , programPath.filename().string());
}

int main(int argc, char* argv[])
{
    try {
        if (argc < 2) {
            printUsage(argv[0]);
            return 1;
        }

        std::string mode = argv[1];
        if (mode == "extract") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            extract(argv[2], argv[3]);
        }
        else if (mode == "pack") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            pack(argv[2], argv[3]);
        }
        else {
            std::print("Invalid mode: {0}\n", mode);
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::print("Error: {0}\n", e.what());
        return 1;
    }
    std::print("Done.\n");
    return 0;
}
