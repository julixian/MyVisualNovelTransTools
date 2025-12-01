#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;

template<typename T>
T read(void* ptr)
{
    T value;
    memcpy(&value, ptr, sizeof(T));
    return value;
}

template<typename T>
void write(void* ptr, T value)
{
    memcpy(ptr, &value, sizeof(T));
}

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
struct FileEntry {
    char fileName[260] = { 0 };
    uint32_t offset = 0;
    uint32_t fileSize = 0;
};
#pragma pack(pop)

constexpr DWORD key1 = 0x627E907B;
constexpr int shift1 = 7;
constexpr DWORD key2 = 0xE1DA85E3;
constexpr int shift2 = 3;

template<DWORD hard_key, int shift>
DWORD decryptDword(DWORD eDword, DWORD dynamic_key) {
    return dynamic_key ^ std::rotr(eDword - hard_key, shift);
}

void extractMode2(std::ifstream& ifs, const fs::path& outputDir) {
    std::ofstream ofs;
    uint32_t indexSize;
    ifs.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));
    std::vector<uint8_t> indexData(indexSize);
    ifs.read(reinterpret_cast<char*>(indexData.data()), indexSize);

    uint32_t indexDataOffset = 0;
    uint32_t decryptMode = 0;
    do {
        DWORD eDword = read<DWORD>(indexData.data() + indexDataOffset);
        DWORD dDword;
        if (decryptMode) {
            dDword = decryptDword<key1, shift1>(eDword, indexSize);
            decryptMode = 0;
        }
        else {
            dDword = decryptDword<key2, shift2>(eDword, indexSize);
            decryptMode = 1;
        }
        write<DWORD>(indexData.data() + indexDataOffset, dDword);
        indexDataOffset++;
    } while (indexDataOffset <= indexSize - 4);

    uint32_t fileCount = indexSize / sizeof(FileEntry);
    for (uint32_t i = 0; i < fileCount; i++) {
        FileEntry fileEntry = read<FileEntry>(indexData.data() + i * sizeof(FileEntry));
        fileEntry.offset += indexSize + 8;
        std::vector<uint8_t> fileData(fileEntry.fileSize);
        ifs.seekg(fileEntry.offset);
        ifs.read(reinterpret_cast<char*>(fileData.data()), fileEntry.fileSize);
        //std::println("FileEntry: {}, offset: {:#x}, size: {:#x}", ascii2Ascii(fileEntry.fileName, 932), fileEntry.offset, fileEntry.fileSize);

        std::wstring fileName = ascii2Wide(fileEntry.fileName, 932);
        const fs::path fileOutputPath = outputDir / fileName;
        if (fileOutputPath.has_parent_path() && !fs::exists(fileOutputPath.parent_path())) {
            fs::create_directories(fileOutputPath.parent_path());
        }
        ofs.open(fileOutputPath, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Failed to open output file: " + wide2Ascii(fileOutputPath));
        }
        ofs.write(reinterpret_cast<const char*>(fileData.data()), fileEntry.fileSize);
        ofs.close();
        std::println("Extracted: {}", wide2Ascii(fileOutputPath));
    }
}

void extractMode1(std::ifstream& ifs, const fs::path& outputDir) {
    std::ofstream ofs;
    uint32_t indexSize;
    ifs.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));

    uint32_t fileCount = indexSize / sizeof(FileEntry);
    ifs.seekg(8);
    std::vector<FileEntry> entries;
    entries.reserve(fileCount);

    for (uint32_t i = 0; i < fileCount; ++i) {
        FileEntry entry;
        ifs.read(reinterpret_cast<char*>(&entry), sizeof(entry));
        entries.push_back(entry);
    }

    uint32_t currentDataOffset = 0;

    for (size_t i = 0; i < entries.size(); ++i) {
        const FileEntry& entry = entries[i];

        uint32_t key = entry.offset ^ currentDataOffset;
        uint32_t trueSize = entry.fileSize ^ key;
        uint32_t trueDataOffset = currentDataOffset + indexSize + 8;

        std::vector<uint8_t> fileData(trueSize);
        ifs.seekg(trueDataOffset);
        ifs.read(reinterpret_cast<char*>(fileData.data()), trueSize);
        //std::println("FileEntry: {}, offset: {:#x}, size: {:#x}", ascii2Ascii(entry.fileName, 932), trueDataOffset, trueSize);

        const fs::path fileOutputPath = outputDir / ascii2Wide(entry.fileName, 932);

        if (fileOutputPath.has_parent_path() && !fs::exists(fileOutputPath.parent_path())) {
            fs::create_directories(fileOutputPath.parent_path());
        }

        ofs.open(fileOutputPath, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Failed to open output file: " + wide2Ascii(fileOutputPath));
        }
        ofs.write(reinterpret_cast<const char*>(fileData.data()), trueSize);
        ofs.close();

        std::println("Extracted: {}", wide2Ascii(fileOutputPath));
        currentDataOffset += trueSize;
    }
}

void extractArchive(const fs::path& archivePath, const fs::path& outputDir) {
    std::ifstream ifs(archivePath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open archive file: " + wide2Ascii(archivePath));
    }

    uint32_t mode;
    ifs.read(reinterpret_cast<char*>(&mode), sizeof(mode));
    if (mode == 0) {
        extractMode1(ifs, outputDir);
    }
    else if (mode == 1) {
        extractMode2(ifs, outputDir);
    }
    else {
        throw std::runtime_error("Unsupported archive mode: " + std::to_string(mode));
    }
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.12.01\n"
        "Usage:\n"
        " For extract: {0} extract <dat_file> <output_dir>\n",
        wide2Ascii(programPath.filename()));
}

int wmain(int argc, wchar_t* argv[]) {

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
            const fs::path archivePath = argv[2];
            const fs::path outputDir = argv[3];
            extractArchive(archivePath, outputDir);
        }
        else {
            std::println("Error: Invalid mode: {}", wide2Ascii(mode));
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
