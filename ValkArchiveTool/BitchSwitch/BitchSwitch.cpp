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

std::vector<uint8_t> string2Bytes(const std::string& str) {
    std::vector<uint8_t> result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
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

DWORD decyptMode1(DWORD eDword, DWORD indexDataSize) {
    return indexDataSize ^ std::rotr(eDword - key1, shift1);
}

DWORD decyptMode2(DWORD eDword, DWORD indexDataSize) {
    return indexDataSize ^ std::rotr(eDword - key2, shift2);
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
            dDword = decyptMode1(eDword, indexSize);
            decryptMode = 0;
        }
        else {
            dDword = decyptMode2(eDword, indexSize);
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

void extractArchive(const fs::path& archivePath, const fs::path& outputDir) {
    std::ifstream ifs(archivePath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open archive file: " + wide2Ascii(archivePath));
    }

    uint32_t mode;
    ifs.read(reinterpret_cast<char*>(&mode), sizeof(mode));
    if (mode == 1) {
        mode = 2;
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
