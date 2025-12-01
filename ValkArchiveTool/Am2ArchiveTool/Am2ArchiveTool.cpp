#include <stdint.h>
#include <Windows.h>

import std;
namespace fs = std::filesystem;

std::string wide2Ascii(const std::wstring& wide, UINT CodePage = CP_UTF8);
std::wstring ascii2Wide(const std::string& ascii, UINT CodePage = CP_ACP);
std::string ascii2Ascii(const std::string& ascii, UINT src = CP_ACP, UINT dst = CP_UTF8);

std::string wide2Ascii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst) {
    return wide2Ascii(ascii2Wide(ascii, src), dst);
}

#pragma pack(push, 1)
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
#pragma pack(pop)

void extractAm2(const fs::path& inputPath, const fs::path& outputDir) {
    std::ifstream ifs(inputPath, std::ios::binary);
    std::ofstream ofs;

    fs::create_directories(outputDir);
    if (!ifs.is_open() || !fs::is_directory(outputDir)) {
        throw std::runtime_error("Failed to open input file or output directory.");
    }

    Am2Header header;
    ifs.read((char*)&header, sizeof(Am2Header));
    uint32_t dataBaseOffset = header.indexSize + 12;

    std::vector<Am2Entry> entries;
    entries.resize(header.fileCount);
    ifs.read((char*)entries.data(), sizeof(Am2Entry) * header.fileCount);

    for (uint32_t i = 0; i < entries.size(); i++) {
        std::wstring fileName = std::format(L"{:04d}.mg2", i);
        ofs.open(outputDir / fileName, std::ios::binary);
        if (!ofs.is_open()) {
            throw std::runtime_error("Failed to open output file: " + wide2Ascii(fileName));
        }
        else {
            std::println("Extracted: {}", wide2Ascii(fileName));
        }
        std::vector<uint8_t> mg2Data(entries[i].size);
        ifs.seekg(dataBaseOffset + entries[i].offset);
        ifs.read((char*)mg2Data.data(), entries[i].size);
        ofs.write((char*)mg2Data.data(), entries[i].size);
        ofs.close();
    }
}

void repackAm2(fs::path inputPath, fs::path inputDir, fs::path outputPath) {
    std::ifstream ifs;
    std::ofstream ofs(outputPath, std::ios::binary);

    if (!fs::is_directory(inputDir)) {
        throw std::runtime_error(wide2Ascii(inputDir) + " is not a directory.");
    }
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open output file: " + wide2Ascii(outputPath));
    }

    ifs.open(inputPath, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open input file: " + wide2Ascii(inputPath));
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
            throw std::runtime_error("Failed to open input file: " + std::format("{:04d}.mg2", i));
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
}


void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.08.20\n"
        "Usage:\n"
        "For extract: {0} extract <input.am2> <output_mg2_dir>\n"
        "For repack: {0} repack <input_org.am2> <input_mg2_dir> <output_new.am2>\n",
        wide2Ascii(programPath.filename()));
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    try {

        if (argc < 2) {
            printUsage(argv[0]);
            return 1;
        }

        std::wstring mode = argv[1];
        if (mode == L"extract") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            extractAm2(argv[2], argv[3]);
        }
        else if (mode == L"repack") {
            if (argc < 5) {
                printUsage(argv[0]);
                return 1;
            }
            repackAm2(argv[2], argv[3], argv[4]);
        }
        else {
            std::println("Invalid mode: {}.", wide2Ascii(mode));
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::println("Error: {}", e.what());
        return 1;
    }

    return 0;
}