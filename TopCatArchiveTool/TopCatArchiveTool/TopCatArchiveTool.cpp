#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;
namespace stdv = std::ranges::views;

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

// -----------------------------------------------------------------------------
// File Structures & Constants
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
struct TcdSectionHeader {
    uint32_t dataSize;
    uint32_t fileCount;
    uint32_t dirCount;
    uint32_t indexOffset;
    uint32_t dirNameLength;
    uint32_t fileNameLength;
    uint32_t tctAbsOffsetCount; // TCT 脚本中绝对跳转的数量
    uint32_t unknownDataSize;
};

struct TcdDirEntry {
    uint32_t fileCount;
    uint32_t namesOffset;
    uint32_t firstIndex;
    uint32_t unknown;
};
#pragma pack(pop)

const std::vector<std::string> SECTION_EXTENSIONS = { ".TCT", ".TSF", ".SPD", ".OGG", ".WAV" };

// -----------------------------------------------------------------------------
// Helper Lambdas/Functions
// -----------------------------------------------------------------------------

void decryptNames(std::vector<char>& buffer, char key) {
    for (char& c : buffer) {
        c -= key;
    }
}

std::string getNameFromBuffer(const std::vector<char>& names, int offset) {
    if (offset < 0 || offset >= names.size()) return "";
    auto it = std::find(names.begin() + offset, names.end(), '\0');
    if (it == names.end()) return "";
    return std::string(names.begin() + offset, it);
}

std::string getNameFromBuffer(const std::vector<char>& names, uint32_t nameLength, uint32_t& offset) {
    char buffer[256] = { 0 };
    memcpy(buffer, &names[offset], nameLength);
    offset += nameLength;
    return std::string(buffer);
}

// -----------------------------------------------------------------------------
// Core Logic
// -----------------------------------------------------------------------------

void extractArchive(const fs::path& archivePath, const fs::path& outputDir) {
    std::ifstream ifs(archivePath, std::ios::binary);
    std::ofstream ofs;
    if (!ifs) {
        throw std::runtime_error("Failed to open TCD file: " + wide2Ascii(archivePath.wstring()));
    }

    // Enable exceptions for read errors
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    uint32_t signature;
    uint32_t totalFiles;
    ifs.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    ifs.read(reinterpret_cast<char*>(&totalFiles), sizeof(totalFiles));

    uint32_t totalSections;
    if (signature == 0x32444354) { // TCD2{
        totalSections = 4;
    }
    else if (signature == 0x33444354) { // TCD3
        totalSections = 5;
    }
    else {
        throw std::runtime_error("Invalid TCD signature.");
    }

    uint32_t currentSectionOffset = 8;

    for (uint32_t i = 0; i < totalSections; ++i) {
        ifs.seekg(currentSectionOffset);

        TcdSectionHeader section;
        ifs.read(reinterpret_cast<char*>(&section.dataSize), sizeof(uint32_t));

        // Store current position to advance to next section later
        uint32_t nextSectionPos = currentSectionOffset + 0x20;

        if (section.dataSize == 0) {
            currentSectionOffset = nextSectionPos;
            continue;
        }

        if (totalSections == 4) {
            ifs.read(reinterpret_cast<char*>(&section.fileCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.dirCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.indexOffset), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.dirNameLength), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.fileNameLength), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.tctAbsOffsetCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.unknownDataSize), sizeof(uint32_t));
        }
        else if (totalSections == 5) {
            ifs.read(reinterpret_cast<char*>(&section.indexOffset), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.dirCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.dirNameLength), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.fileCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.fileNameLength), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.tctAbsOffsetCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.unknownDataSize), sizeof(uint32_t));
        }

        // Read Directory Names
        ifs.seekg(section.indexOffset);
        std::vector<char> dirNames(section.dirNameLength * (totalSections == 4 ? 1 : section.dirCount));
        ifs.read(dirNames.data(), dirNames.size());

        char sectionKey = dirNames.back();
        decryptNames(dirNames, sectionKey);

        // Read Directory Entries
        std::vector<TcdDirEntry> dirs(section.dirCount);
        for (auto& dir : dirs) {
            ifs.read(reinterpret_cast<char*>(&dir), sizeof(TcdDirEntry));
        }

        // Read File Names
        std::vector<char> fileNames(section.fileNameLength * (totalSections == 4 ? 1 : section.fileCount));
        ifs.read(fileNames.data(), fileNames.size());
        decryptNames(fileNames, sectionKey);

        // Read Offsets
        std::vector<uint32_t> offsets(section.fileCount + 1);
        ifs.read(reinterpret_cast<char*>(offsets.data()), offsets.size() * sizeof(uint32_t));

        if (i == 0 && section.tctAbsOffsetCount > 0) {
            std::vector<uint32_t> tctAbsOffsetsIndex(section.fileCount + 1);
            ifs.read(reinterpret_cast<char*>(tctAbsOffsetsIndex.data()), tctAbsOffsetsIndex.size() * sizeof(uint32_t));
            std::vector<uint32_t> tctAbsOffsetsTable(section.tctAbsOffsetCount);
            ifs.read(reinterpret_cast<char*>(tctAbsOffsetsTable.data()), tctAbsOffsetsTable.size() * sizeof(uint32_t));
            ofs.open(L"tct_abs_offsets.txt");
            if (!ofs) {
                throw std::runtime_error("Failed to open TCT absolute offset file.");
            }
            for (uint32_t absOffset : tctAbsOffsetsTable) {
                ofs << std::format("{:08X}\n", absOffset);
            }
            ofs.close();
            std::vector<uint8_t> unknownData(section.unknownDataSize * (totalSections == 4 ? 1 : section.tctAbsOffsetCount));
            ifs.read(reinterpret_cast<char*>(unknownData.data()), unknownData.size());
            uint8_t unkownDataKey = unknownData.back();
            for (uint8_t& c : unknownData) {
                c -= unkownDataKey;
            }
            ofs.open(L"tct_unknown_data.bin", std::ios::binary);
            if (!ofs) {
                throw std::runtime_error("Failed to open TCT unknown data file.");
            }
            ofs.write(reinterpret_cast<char*>(unknownData.data()), unknownData.size());
            ofs.close();
        }

        // Process Directories and Files
        uint32_t dirNameOffset = 0;
        for (const auto& dir : dirs) {
            std::string dirName;
            if (totalSections == 4) {
                dirName = getNameFromBuffer(dirNames, dirNameOffset);
                dirNameOffset += static_cast<uint32_t>(dirName.length() + 1);
            }
            else if (totalSections == 5) {
                dirName = getNameFromBuffer(dirNames, section.dirNameLength, dirNameOffset);
            }

            uint32_t fileIndex = dir.firstIndex;
            uint32_t fileNameOffset = dir.namesOffset;

            for (uint32_t k = 0; k < dir.fileCount; ++k) {
                std::string fileName;
                if (totalSections == 4) {
                    fileName = getNameFromBuffer(fileNames, fileNameOffset);
                    fileNameOffset += static_cast<uint32_t>(fileName.length() + 1);
                }
                else if (totalSections == 5) {
                    fileName = getNameFromBuffer(fileNames, section.fileNameLength, fileNameOffset);
                }

                // Construct path and enforce extension
                fs::path relPath = fs::path(ascii2Wide(dirName, 932)) / ascii2Wide(fileName, 932);
                relPath.replace_extension(SECTION_EXTENSIONS[i]);

                fs::path fullPath = outputDir / relPath;

                // Create directory if needed
                if (!fs::exists(fullPath.parent_path())) {
                    fs::create_directories(fullPath.parent_path());
                }

                uint32_t fileOffset = offsets[fileIndex];
                uint32_t fileSize = offsets[fileIndex + 1] - offsets[fileIndex];

                // Extract data
                std::vector<char> dataBuffer(fileSize);

                ifs.seekg(fileOffset);
                ifs.read(dataBuffer.data(), fileSize);

                ofs.open(fullPath, std::ios::binary);
                if (!ofs) {
                    throw std::runtime_error(std::format("Failed to open output file: {}", wide2Ascii(fullPath.wstring())));
                }
                else {
                    ofs.write(dataBuffer.data(), fileSize);
                    std::println("Extracted: {}", wide2Ascii(relPath.wstring()));
                }
                ofs.close();

                fileIndex++;
            }
        }

        currentSectionOffset = nextSectionPos;
    }
}

void repackArchive(const fs::path& origArchivePath, const fs::path& modifiedDir, const fs::path& outputArchivePath, std::optional<fs::path> tctAbsOffsetFile) {

    std::ifstream ifs(origArchivePath, std::ios::binary);
    std::ofstream ofs(outputArchivePath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open TCD file: " + wide2Ascii(origArchivePath.wstring()));
    }
    if (!ofs) {
        throw std::runtime_error("Failed to open output TCD file: " + wide2Ascii(outputArchivePath.wstring()));
    }

    // Enable exceptions for read errors
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    uint32_t signature;
    uint32_t totalFiles;
    ifs.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    ifs.read(reinterpret_cast<char*>(&totalFiles), sizeof(totalFiles));
    ofs.write(reinterpret_cast<char*>(&signature), sizeof(signature));
    ofs.write(reinterpret_cast<char*>(&totalFiles), sizeof(totalFiles));

    uint32_t totalSections;
    if (signature == 0x32444354) { // TCD2{
        totalSections = 4;
    }
    else if (signature == 0x33444354) { // TCD3
        totalSections = 5;
    }
    else {
        throw std::runtime_error("Invalid TCD signature.");
    }

    uint32_t currentSectionOffset = 8;
    for (uint32_t i = 0; i < totalSections; ++i) {
        TcdSectionHeader section = { 0 };
        ofs.write(reinterpret_cast<char*>(&section), sizeof(TcdSectionHeader));
    }

    for (uint32_t i = 0; i < totalSections; ++i) {
        ifs.seekg(currentSectionOffset);

        TcdSectionHeader section;
        ifs.read(reinterpret_cast<char*>(&section.dataSize), sizeof(uint32_t));

        // Store current position to advance to next section later
        uint32_t nextSectionPos = currentSectionOffset + 0x20;

        if (section.dataSize == 0) {
            currentSectionOffset = nextSectionPos;
            continue;
        }

        if (totalSections == 4) {
            ifs.read(reinterpret_cast<char*>(&section.fileCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.dirCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.indexOffset), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.dirNameLength), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.fileNameLength), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.tctAbsOffsetCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.unknownDataSize), sizeof(uint32_t));
        }
        else if (totalSections == 5) {
            ifs.read(reinterpret_cast<char*>(&section.indexOffset), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.dirCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.dirNameLength), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.fileCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.fileNameLength), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.tctAbsOffsetCount), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&section.unknownDataSize), sizeof(uint32_t));
        }

        TcdSectionHeader newSectionHeader = section;
        std::vector<uint8_t> newIndexData;
        std::vector<std::vector<uint8_t>> newFileData(section.fileCount);

        // Read Directory Names
        ifs.seekg(section.indexOffset);
        std::vector<char> dirNames(section.dirNameLength * (totalSections == 4 ? 1 : section.dirCount));
        ifs.read(dirNames.data(), dirNames.size());
        newIndexData.insert(newIndexData.end(), dirNames.begin(), dirNames.end());

        char sectionKey = dirNames.back();
        decryptNames(dirNames, sectionKey);

        // Read Directory Entries
        std::vector<TcdDirEntry> dirs(section.dirCount);
        for (auto& dir : dirs) {
            ifs.read(reinterpret_cast<char*>(&dir), sizeof(TcdDirEntry));
            newIndexData.insert(newIndexData.end(), reinterpret_cast<uint8_t*>(&dir), reinterpret_cast<uint8_t*>(&dir) + sizeof(TcdDirEntry));
        }

        // Read File Names
        std::vector<char> fileNames(section.fileNameLength * (totalSections == 4 ? 1 : section.fileCount));
        ifs.read(fileNames.data(), fileNames.size());
        newIndexData.insert(newIndexData.end(), fileNames.begin(), fileNames.end());
        decryptNames(fileNames, sectionKey);

        // Read Offsets
        std::vector<uint32_t> offsets(section.fileCount + 1);
        ifs.read(reinterpret_cast<char*>(offsets.data()), offsets.size() * sizeof(uint32_t));
        uint32_t fileOffsetIndex = (uint32_t)newIndexData.size();
        newIndexData.insert(newIndexData.end(), reinterpret_cast<uint8_t*>(offsets.data()), reinterpret_cast<uint8_t*>(offsets.data()) + offsets.size() * sizeof(uint32_t));

        bool hasTctAbsOffsets = (i == 0 && section.tctAbsOffsetCount > 0);
        if (hasTctAbsOffsets) {
            std::vector<uint32_t> tctAbsOffsetsIndex(section.fileCount + 1);
            ifs.read(reinterpret_cast<char*>(tctAbsOffsetsIndex.data()), tctAbsOffsetsIndex.size() * sizeof(uint32_t));
            newIndexData.insert(newIndexData.end(), reinterpret_cast<uint8_t*>(tctAbsOffsetsIndex.data()), reinterpret_cast<uint8_t*>(tctAbsOffsetsIndex.data()) + tctAbsOffsetsIndex.size() * sizeof(uint32_t));
            std::vector<uint32_t> tctAbsOffsetsTable(section.tctAbsOffsetCount);
            ifs.read(reinterpret_cast<char*>(tctAbsOffsetsTable.data()), tctAbsOffsetsTable.size() * sizeof(uint32_t));
            if (tctAbsOffsetFile.has_value()) {
                std::ifstream tctAbsOffsetInStream(tctAbsOffsetFile.value());
                if (!tctAbsOffsetInStream) {
                    throw std::runtime_error("Failed to open TCT absolute offset file.");
                }
                std::string line;
                std::vector<uint32_t> newTctAbsOffsetsTable;
                while (std::getline(tctAbsOffsetInStream, line)) {
                    uint32_t absOffset = std::stoul(line, nullptr, 16);
                    newTctAbsOffsetsTable.push_back(absOffset);
                }
                if (newTctAbsOffsetsTable.size() != section.tctAbsOffsetCount) {
                    throw std::runtime_error(std::format("TCT absolute offset file has incorrect size (expected {}, got {}).", section.tctAbsOffsetCount, newTctAbsOffsetsTable.size()));
                }
                tctAbsOffsetsTable = std::move(newTctAbsOffsetsTable);
            }
            newIndexData.insert(newIndexData.end(), reinterpret_cast<uint8_t*>(tctAbsOffsetsTable.data()), reinterpret_cast<uint8_t*>(tctAbsOffsetsTable.data()) + tctAbsOffsetsTable.size() * sizeof(uint32_t));
            std::vector<uint8_t> unknownData(section.unknownDataSize * (totalSections == 4 ? 1 : section.tctAbsOffsetCount));
            ifs.read(reinterpret_cast<char*>(unknownData.data()), unknownData.size());
            newIndexData.insert(newIndexData.end(), unknownData.begin(), unknownData.end());
            std::vector<uint32_t> unknownTable(section.fileCount + 1);
            ifs.read(reinterpret_cast<char*>(unknownTable.data()), unknownTable.size() * sizeof(uint32_t));
            newIndexData.insert(newIndexData.end(), reinterpret_cast<uint8_t*>(unknownTable.data()), reinterpret_cast<uint8_t*>(unknownTable.data()) + unknownTable.size() * sizeof(uint32_t));
        }

        // Process Directories and Files
        uint32_t dirNameOffset = 0;
        for (const auto& dir : dirs) {
            std::string dirName;
            if (totalSections == 4) {
                dirName = getNameFromBuffer(dirNames, dirNameOffset);
                dirNameOffset += static_cast<uint32_t>(dirName.length() + 1);
            }
            else if (totalSections == 5) {
                dirName = getNameFromBuffer(dirNames, section.dirNameLength, dirNameOffset);
            }

            uint32_t fileIndex = dir.firstIndex;
            uint32_t fileNameOffset = dir.namesOffset;

            for (uint32_t k = 0; k < dir.fileCount; ++k) {
                std::string fileName;
                if (totalSections == 4) {
                    fileName = getNameFromBuffer(fileNames, fileNameOffset);
                    fileNameOffset += static_cast<uint32_t>(fileName.length() + 1);
                }
                else if (totalSections == 5) {
                    fileName = getNameFromBuffer(fileNames, section.fileNameLength, fileNameOffset);
                }

                // Construct path and enforce extension
                fs::path relPath = fs::path(ascii2Wide(dirName, 932)) / ascii2Wide(fileName, 932);
                relPath.replace_extension(SECTION_EXTENSIONS[i]);

                fs::path fullPath = modifiedDir / relPath;

                if (fs::exists(fullPath)) {
                    std::println("Replacing: {}", wide2Ascii(relPath.wstring()));
                    std::ifstream newFile(fullPath, std::ios::binary);
                    if (!newFile) {
                        throw std::runtime_error(std::format("Failed to open input file: {}", wide2Ascii(fullPath.wstring())));
                    }
                    newFileData[fileIndex].resize(fs::file_size(fullPath));
                    newFile.read((char*)newFileData[fileIndex].data(), newFileData[fileIndex].size());
                    newFile.close();
                }
                else {
                    std::println("Using original: {}", wide2Ascii(relPath.wstring()));
                    uint32_t fileOffset = offsets[fileIndex];
                    uint32_t fileSize = offsets[fileIndex + 1] - offsets[fileIndex];
                    newFileData[fileIndex].resize(fileSize);
                    ifs.seekg(fileOffset);
                    ifs.read((char*)newFileData[fileIndex].data(), newFileData[fileIndex].size());
                }

                fileIndex++;
            }
        }

        ofs.seekp(0, std::ios::end);
        offsets[0] = (uint32_t)ofs.tellp();
        for (uint32_t j = 1; j < offsets.size(); ++j) {
            offsets[j] = offsets[j - 1] + (uint32_t)newFileData[j - 1].size();
        }
        memcpy(newIndexData.data() + fileOffsetIndex, offsets.data(), offsets.size() * sizeof(uint32_t));
        uint32_t newDataSize = (uint32_t)std::ranges::fold_left(newFileData | stdv::transform(&std::vector<uint8_t>::size), 0uz, std::plus<>{});
        newSectionHeader.dataSize = newDataSize;
        newSectionHeader.indexOffset = (uint32_t)ofs.tellp() + newDataSize;
        for (const auto& fileData : newFileData) {
            ofs.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
        }
        ofs.write(reinterpret_cast<const char*>(newIndexData.data()), newIndexData.size());
        ofs.seekp(currentSectionOffset);
        if (totalSections == 4) {
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.dataSize), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.fileCount), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.dirCount), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.indexOffset), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.dirNameLength), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.fileNameLength), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.tctAbsOffsetCount), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.unknownDataSize), sizeof(uint32_t));
        }
        else if (totalSections == 5) {
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.dataSize), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.indexOffset), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.dirCount), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.dirNameLength), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.fileCount), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.fileNameLength), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.tctAbsOffsetCount), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&newSectionHeader.unknownDataSize), sizeof(uint32_t));
        }

        currentSectionOffset = nextSectionPos;
    }

    ifs.close();
    ofs.close();

    std::println("Repacked: {}", wide2Ascii(outputArchivePath.wstring()));
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.22\n"
        "Usage:\n"
        " For extract: {0} extract <tcd_file> <output_dir>\n"
        " For repack: {0} repack <orig_tcd_file> <modified_dir> <output_tcd_file> [tct_abs_offset_file]\n",
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
        else if (mode == L"repack") {
            if (argc < 5) {
                printUsage(argv[0]);
                return 1;
            }
            const fs::path origArchivePath = argv[2];
            const fs::path modifiedDir = argv[3];
            const fs::path outputArchivePath = argv[4];
            std::optional<fs::path> tctAbsOffsetFile;
            if (argc >= 6) {
                tctAbsOffsetFile = argv[5];
            }
            repackArchive(origArchivePath, modifiedDir, outputArchivePath, tctAbsOffsetFile);
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
