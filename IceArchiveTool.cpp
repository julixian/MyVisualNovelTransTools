#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace fs = std::filesystem;

struct Entry {
    uint32_t offset;
    uint32_t size;
    std::string name;
};

class BinManager {
private:
    std::vector<Entry> m_entries;
    fs::path m_binPath;
    fs::path m_folderPath;
    fs::path m_outputPath;  // 新增：输出文件路径

    void logError(const std::string& message) {
        std::cerr << "Error: " << message << std::endl;
    }

    void logInfo(const std::string& message) {
        std::cout << "Info: " << message << std::endl;
    }

    bool readBinStructure() {
        std::ifstream file(m_binPath, std::ios::binary);
        if (!file) {
            logError("Failed to open bin file: " + m_binPath.string());
            return false;
        }

        uint32_t first_offset;
        file.read(reinterpret_cast<char*>(&first_offset), sizeof(first_offset));

        if (first_offset < 8 || first_offset >= fs::file_size(m_binPath) || 0 != (first_offset & 3)) {
            logError("Invalid bin file structure. First offset: " + std::to_string(first_offset));
            return false;
        }

        int count = (first_offset - 4) / 4;
        if (count <= 0 || count > 10000) {
            logError("Invalid file count: " + std::to_string(count));
            return false;
        }

        m_entries.reserve(count);

        uint32_t prevOffset = first_offset;
        for (int i = 0; i < count; ++i) {
            uint32_t nextOffset;
            file.read(reinterpret_cast<char*>(&nextOffset), sizeof(nextOffset));

            if (nextOffset < prevOffset) {
                logError("Invalid offset order at index " + std::to_string(i));
                return false;
            }

            Entry entry;
            entry.offset = prevOffset;
            entry.size = nextOffset - prevOffset;
            entry.name = "file_" + std::to_string(i) + ".bin";

            m_entries.push_back(entry);
            prevOffset = nextOffset;
        }

        logInfo("Successfully read " + std::to_string(m_entries.size()) + " entries from bin file");
        return true;
    }

    bool extractFiles() {
        std::ifstream binFile(m_binPath, std::ios::binary);
        if (!binFile) {
            logError("Failed to open bin file for reading: " + m_binPath.string());
            return false;
        }

        for (const auto& entry : m_entries) {
            fs::path outputPath = m_folderPath / entry.name;
            std::ofstream outFile(outputPath, std::ios::binary);
            if (!outFile) {
                logError("Failed to create output file: " + outputPath.string());
                return false;
            }

            binFile.seekg(entry.offset);
            std::vector<char> buffer(entry.size);
            binFile.read(buffer.data(), entry.size);
            outFile.write(buffer.data(), entry.size);

            logInfo("Extracted: " + entry.name);
        }

        return true;
    }

    std::vector<fs::path> getUpdateFiles() {
        std::vector<fs::path> updateFiles;
        for (const auto& entry : fs::directory_iterator(m_folderPath)) {
            if (entry.is_regular_file()) {
                updateFiles.push_back(entry.path());
            }
        }

        std::sort(updateFiles.begin(), updateFiles.end(), [this](const fs::path& a, const fs::path& b) {
            std::string nameA = a.stem().string();
            std::string nameB = b.stem().string();

            logInfo("Sorting file: " + nameA);

            auto extractNumber = [this](const std::string& name) -> int {
                size_t pos = name.find_last_of('_');
                if (pos != std::string::npos && pos + 1 < name.length()) {
                    try {
                        return std::stoi(name.substr(pos + 1));
                    }
                    catch (const std::exception& e) {
                        logError("Failed to extract number from filename: " + name + ". Error: " + e.what());
                        return -1;
                    }
                }
                logError("Invalid filename format: " + name);
                return -1;
                };

            int numA = extractNumber(nameA);
            int numB = extractNumber(nameB);

            if (numA == -1 || numB == -1) {
                return nameA < nameB;
            }

            return numA < numB;
            });

        logInfo("Found " + std::to_string(updateFiles.size()) + " update files");
        for (const auto& file : updateFiles) {
            logInfo("Update file: " + file.filename().string());
        }
        return updateFiles;
    }

    bool updateBinFile() {
        std::ifstream originalBin(m_binPath, std::ios::binary);
        std::ofstream updatedBin(m_outputPath, std::ios::binary);  // 使用新的输出路径

        if (!originalBin) {
            logError("Failed to open original bin file for reading: " + m_binPath.string());
            return false;
        }

        if (!updatedBin) {
            logError("Failed to open output bin file for writing: " + m_outputPath.string());
            return false;
        }

        auto updateFiles = getUpdateFiles();

        // Write the initial offset table
        uint32_t currentOffset = 4 + m_entries.size() * 4;
        updatedBin.write(reinterpret_cast<char*>(&currentOffset), sizeof(currentOffset));

        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (i < updateFiles.size()) {
                uint32_t newSize = static_cast<uint32_t>(fs::file_size(updateFiles[i]));
                currentOffset += newSize;
            }
            else {
                currentOffset += m_entries[i].size;
            }

            updatedBin.write(reinterpret_cast<char*>(&currentOffset), sizeof(currentOffset));
        }

        // Write file contents
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (i < updateFiles.size()) {
                std::ifstream updateFile(updateFiles[i], std::ios::binary);
                if (!updateFile) {
                    logError("Failed to open update file: " + updateFiles[i].string());
                    return false;
                }
                updatedBin << updateFile.rdbuf();
                logInfo("Updated file " + std::to_string(i) + " with " + updateFiles[i].string());
            }
            else {
                originalBin.seekg(m_entries[i].offset);
                std::vector<char> buffer(m_entries[i].size);
                originalBin.read(buffer.data(), m_entries[i].size);
                updatedBin.write(buffer.data(), m_entries[i].size);
                logInfo("Kept original file " + std::to_string(i));
            }
        }

        logInfo("Bin file update completed successfully. Output: " + m_outputPath.string());
        return true;
    }

public:
    BinManager(const fs::path& binPath, const fs::path& folderPath, const fs::path& outputPath = "")
        : m_binPath(binPath), m_folderPath(folderPath), m_outputPath(outputPath) {}

    bool Extract() {
        try {
            if (!readBinStructure()) {
                return false;
            }

            return extractFiles();
        }
        catch (const std::exception& e) {
            logError("Unexpected error during extraction: " + std::string(e.what()));
            return false;
        }
    }

    bool Update() {
        try {
            if (!readBinStructure()) {
                return false;
            }

            return updateBinFile();
        }
        catch (const std::exception& e) {
            logError("Unexpected error during update: " + std::string(e.what()));
            return false;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Made by julixian 2025.01.06" << std::endl;
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  Extract: " << argv[0] << " extract <bin_file> <output_folder>" << std::endl;
        std::cerr << "  Pack:    " << argv[0] << " pack <orgi_bin_file> <input_folder> <output_bin_file>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    fs::path binPath = argv[2];
    fs::path folderPath = argv[3];
    fs::path outputPath;

    if (!fs::exists(binPath) || !fs::is_regular_file(binPath)) {
        std::cerr << "Error: Bin file does not exist or is not a regular file." << std::endl;
        return 1;
    }

    if (mode == "extract") {
        if (!fs::exists(folderPath)) {
            fs::create_directories(folderPath);
        }
    }
    else if (mode == "pack") {
        if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
            std::cerr << "Error: Pack folder does not exist or is not a directory." << std::endl;
            return 1;
        }
        if (argc != 5) {
            std::cerr << "Error: Pack mode requires an output bin file path." << std::endl;
            return 1;
        }
        outputPath = argv[4];
    }
    else {
        std::cerr << "Error: Invalid mode. Use 'extract' or 'pack'." << std::endl;
        return 1;
    }

    BinManager manager(binPath, folderPath, outputPath);
    bool success;

    if (mode == "extract") {
        success = manager.Extract();
    }
    else {
        success = manager.Update();
    }

    if (success) {
        std::cout << "Operation completed successfully." << std::endl;
        return 0;
    }
    else {
        std::cerr << "Operation failed. Check the error messages above for details." << std::endl;
        return 1;
    }
}
