#include <Windows.h>
#include <cstdint>

import std;
import nlohmann.json;
namespace fs = std::filesystem;
using json = nlohmann::json;

extern "C" size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);
extern "C" size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);

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

std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep) {
    str = str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::string>();
    return str;
}

std::string replaceStr(std::string_view str, std::string_view org, std::string_view rep) {
    std::string result = str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::string>();
    return result;
}

void decompress(const fs::path& inputScbPath, const fs::path& outputPath) {
    std::ifstream ifs(inputScbPath, std::ios::binary);
    std::ofstream ofs(outputPath, std::ios::binary);
    if (!ifs.is_open() || !ofs.is_open()) {
        throw std::runtime_error("Failed to open file.");
    }

    uint64_t fileSize = fs::file_size(inputScbPath);

    uint32_t magic;
    ifs.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x4B504C47) { // GLPK
        throw std::runtime_error("Invalid magic number.");
    }
    uint32_t decompressedSize;
    uint32_t version;
    ifs.read(reinterpret_cast<char*>(&decompressedSize), 4);
    ifs.read(reinterpret_cast<char*>(&version), 4);

    std::vector<uint8_t> compressedData(fileSize - 12);
    ifs.read(reinterpret_cast<char*>(compressedData.data()), compressedData.size());
    ifs.close();
    for (auto& byte : compressedData) {
        byte = ~byte;
    }

    std::string decompressedData(decompressedSize, '\0');
    size_t actualSize = lzss_decompress((uint8_t*)decompressedData.data(), decompressedSize, compressedData.data(), compressedData.size());
    if (actualSize != decompressedSize) {
        throw std::runtime_error("Failed to decompress data.");
    }
    
    const fs::path inputSf0Path = fs::path(inputScbPath).replace_extension(L".sf0");
    ifs.open(inputSf0Path, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open sf0 file.");
    }
    uint32_t offsetCount = 0;
    ifs.read(reinterpret_cast<char*>(&offsetCount), 4);
    std::vector<uint32_t> offsets(offsetCount);
    ifs.read(reinterpret_cast<char*>(offsets.data()), offsetCount * 4);
    ifs.close();
    for (const auto& offset : offsets | std::views::reverse) {
        if (offset > decompressedSize) {
            break;
        }
        decompressedData.insert(offset, "__SF0__SIG__");
    }

    ofs.write(reinterpret_cast<char*>(decompressedData.data()), decompressedData.size());
    ofs.close();

    std::println("Decompressed {}", wide2Ascii(inputScbPath));
}

void compress(const fs::path& inputScbPath, const fs::path& outputPath, const fs::path& orgSf0Path, const fs::path& outputSf0Path) {
    std::ifstream ifs(inputScbPath, std::ios::binary);
    std::ofstream ofs(outputPath, std::ios::binary);
    if (!ifs.is_open() || !ofs.is_open()) {
        throw std::runtime_error("Failed to open file.");
    }

    uint32_t fileSize = (uint32_t)fs::file_size(inputScbPath);
    uint32_t magic = 0x4B504C47; // GLPK
    uint32_t version = 1;

    std::string decompressedData(fileSize, '\0');
    ifs.read(reinterpret_cast<char*>(decompressedData.data()), fileSize);
    ifs.close();
    std::vector<uint32_t> offsets;
    std::string newData;
    newData.reserve(decompressedData.size());
    size_t currentPos = 0;
    size_t sigPos = decompressedData.find("__SF0__SIG__");
    while (sigPos != std::string::npos) {
        newData.append(decompressedData.substr(currentPos, sigPos - currentPos));
        offsets.push_back(newData.size());
        currentPos = sigPos + 12;
        sigPos = decompressedData.find("__SF0__SIG__", currentPos);
    }
    newData.append(decompressedData.substr(currentPos));
    decompressedData = std::move(newData);

    std::vector<uint8_t> compressedData((decompressedData.size() + 7) / 8 * 9);
    size_t actualSize = lzss_compress(compressedData.data(), compressedData.size(), (uint8_t*)decompressedData.data(), decompressedData.size());
    if (actualSize == (size_t)-1) {
        throw std::runtime_error("Failed to compress data.");
    }
    compressedData.resize(actualSize);

    for (auto& byte : compressedData) {
        byte = ~byte;
    }
    ofs.write(reinterpret_cast<char*>(&magic), 4);
    uint32_t decompressedSize = (uint32_t)decompressedData.size();
    ofs.write(reinterpret_cast<char*>(&decompressedSize), 4);
    ofs.write(reinterpret_cast<char*>(&version), 4);
    ofs.write(reinterpret_cast<char*>(compressedData.data()), compressedData.size());
    ofs.close();

    
    ifs.open(orgSf0Path, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open sf0 file: " + wide2Ascii(orgSf0Path));
    }
    ofs.open(outputSf0Path, std::ios::binary);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open sf0 file: " + wide2Ascii(outputSf0Path));
    }
    if (offsets.empty()) {
        ofs << ifs.rdbuf();
    }
    else {
        uint32_t orgOffsetCount;
        ifs.read(reinterpret_cast<char*>(&orgOffsetCount), 4);
        if (orgOffsetCount != offsets.size()) {
            throw std::runtime_error(std::format("Offset count mismatch: {} vs {}", orgOffsetCount, offsets.size()));
        }
        ofs.write(reinterpret_cast<char*>(&orgOffsetCount), 4);
        ofs.write(reinterpret_cast<char*>(offsets.data()), offsets.size() * 4);
    }
    ifs.close();
    ofs.close();

    std::println("Compressed {}", wide2Ascii(inputScbPath));
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.12.29\n"
        "Usage: \n"
        "  Decompress: {0} decompress <scb_dir> <output_dir>\n"
        "  Compress: {0} compress <org_scb_dir> <decompressed_scb_dir> <output_dir> \n",
        wide2Ascii(programPath.filename()));
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        std::wstring mode = argv[1];
        if (mode == L"decompress") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            const fs::path inputScbDir = argv[2];
            const fs::path outputDir = argv[3];
            fs::create_directories(outputDir);
            for (const auto& entry : fs::recursive_directory_iterator(inputScbDir)) {
                if (entry.is_regular_file() && entry.path().extension() == L".scb") {
                    const fs::path inputPath = entry.path();
                    const fs::path outputPath = outputDir / fs::relative(inputPath, inputScbDir);
                    if (!fs::exists(outputPath.parent_path())) {
                        fs::create_directories(outputPath.parent_path());
                    }
                    decompress(inputPath, outputPath);
                }
            }
        }
        else if (mode == L"compress") {
            if (argc < 5) {
                printUsage(argv[0]);
                return 1;
            }
            std::ifstream ifs;
            std::ofstream ofs;
            const fs::path orgScbDir = argv[2];
            const fs::path inputScbDir = argv[3];
            const fs::path outputDir = argv[4];
            fs::create_directories(outputDir);
            for (const auto& entry : fs::recursive_directory_iterator(inputScbDir)) {
                if (entry.is_regular_file() && entry.path().extension() == L".scb") {
                    const fs::path inputPath = entry.path();
                    const fs::path outputPath = outputDir / fs::relative(inputPath, inputScbDir);
                    const fs::path orgSf0Path = orgScbDir / fs::relative(inputPath, inputScbDir).replace_extension(L".sf0");
                    const fs::path outputSf0Path = outputDir / fs::relative(inputPath, inputScbDir).replace_extension(L".sf0");
                    if (!fs::exists(outputPath.parent_path())) {
                        fs::create_directories(outputPath.parent_path());
                    }
                    compress(inputPath, outputPath, orgSf0Path, outputSf0Path);
                }
            }
        }
        else {
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