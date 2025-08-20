#include <stdint.h>
#include <Windows.h>

import std;
namespace fs = std::filesystem;

struct Am2Header {
    uint32_t indexSize;
    uint32_t fileCount;
    uint32_t reserved;
};

struct Am2Entry {
    uint32_t offset;
    uint32_t size;
    uint32_t frameDuration; // ms
};

bool extractAm2(fs::path inputPath, fs::path outputDir) {
    std::ifstream ifs(inputPath, std::ios::binary);
    std::ofstream ofs;

    fs::create_directories(outputDir);
    if (!ifs.is_open() || !fs::is_directory(outputDir)) {
        std::print("Failed to open input file or output directory.\n");
        return false;
    }

    Am2Header header;
    ifs.read((char*)&header, sizeof(Am2Header));
    uint32_t dataBaseOffset = header.indexSize + 12;

    std::vector<Am2Entry> entries;
    entries.resize(header.fileCount);
    ifs.read((char*)entries.data(), sizeof(Am2Entry) * header.fileCount);

    for (uint32_t i = 0; i < entries.size(); i++) {
        ofs.open(outputDir / std::format(L"{:04d}.mg2", i), std::ios::binary);
        if (!ofs.is_open()) {
            std::print("Failed to open output file: {}.\n", std::format("{:04d}.mg2", i));
            continue;
        }
        else {
            std::print("Extracting file: {}\n", std::format("{:04d}.mg2", i));
        }
        std::vector<uint8_t> mg2Data(entries[i].size);
        ifs.seekg(dataBaseOffset + entries[i].offset);
        ifs.read((char*)mg2Data.data(), entries[i].size);
        ofs.write((char*)mg2Data.data(), entries[i].size);
        ofs.close();
    }

    return true;
}

bool repackAm2(fs::path inputPath, fs::path inputDir, fs::path outputPath) {
    std::ifstream ifs;
    std::ofstream ofs(outputPath, std::ios::binary);

    if (!fs::is_directory(inputDir)) {
        std::print("{} is not a directory.\n", inputDir.string());
        return false;
    }
    if (!ofs.is_open()) {
        std::print("Failed to open file: {}\n", outputPath.string());
        return false;
    }

    ifs.open(inputPath, std::ios::binary);
    if (!ifs.is_open()) {
        std::print("Failed to open file: {}\n", inputPath.string());
        return false;
    }

    Am2Header header;
    ifs.read((char*)&header, sizeof(Am2Header));
    ofs.write((char*)&header, sizeof(Am2Header));
    ofs.seekp(header.indexSize + 12);

    std::vector<Am2Entry> entries;
    entries.resize(header.fileCount);
    ifs.read((char*)entries.data(), sizeof(Am2Entry) * header.fileCount);
    ifs.close();

    uint32_t currentOffset = 0;
    for (uint32_t i = 0; i < entries.size(); i++) {
        ifs.open(inputDir / std::format(L"{:04d}.mg2", i), std::ios::binary);
        if (!ifs.is_open()) {
            std::print("Failed to open input file: {}.\n", std::format("{:04d}.mg2", i));
            return false;
        }
        std::vector<uint8_t> newMg2Data(fs::file_size(inputDir / std::format(L"{:04d}.mg2", i)));
        ifs.read((char*)newMg2Data.data(), newMg2Data.size());
        entries[i].offset = currentOffset;
        entries[i].size = newMg2Data.size();
        ofs.write((char*)newMg2Data.data(), newMg2Data.size());
        currentOffset += newMg2Data.size();
        ifs.close();
    }

    ofs.seekp(12);
    ofs.write((char*)entries.data(), sizeof(Am2Entry) * header.fileCount);
    ofs.close();

    return true;
}


void printUsage(fs::path programName) {
    std::print("Made by julixian 2025.08.20\n"
        "Usage:\n"
        "For extract: {0} extract <input.am2> <output_mg2_dir>\n"
        "For repack: {0} repack <input_org.am2> <input_mg2_dir> <output_new.am2>\n", programName.filename().string());
}

int main(int argc, char* argv[])
{
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "extract") {
        if (argc < 4) {
            printUsage(argv[0]);
            return 1;
        }
        if (!extractAm2(argv[2], argv[3])) {
            std::print("Failed to extract Am2 file.\n");
            return 1;
        }
        else {
            std::print("Extracted Am2 file successfully.\n");
            return 0;
        }
    }
    else if (mode == "repack") {
        if (argc < 5) {
            printUsage(argv[0]);
            return 1;
        }
        if (!repackAm2(argv[2], argv[3], argv[4])) {
            std::print("Failed to repack Am2 file.\n");
            fs::remove(argv[4]);
            return 1;
        }
        else {
            std::print("Repacked Am2 file successfully.\n");
            return 0;
        }
    }
    else {
        std::print("Invalid mode.\n");
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}