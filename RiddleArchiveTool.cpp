#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

struct Entry {
    std::string name;
    uint32_t size;
    uint32_t offset;
    bool isPacked;
    uint32_t unpackedSize;
    std::vector<char> originalHeader;
};

class PacExtractor {
private:
    std::ifstream file;
    std::vector<Entry> directory;

    bool readDirectory() {
        uint32_t signature;
        file.read(reinterpret_cast<char*>(&signature), 4);
        if (signature != 0x31434150) return false; // 'PAC1'

        int32_t count;
        file.read(reinterpret_cast<char*>(&count), 4);
        if (count < 0 || count > 10000) return false; // Sanity check

        uint32_t indexOffset = 8;
        uint32_t baseOffset = count * 0x20 + 8;

        for (int i = 0; i < count; ++i) {
            Entry entry;
            char name[17] = { 0 };
            file.seekg(indexOffset);
            file.read(name, 16);
            entry.name = name;

            file.read(reinterpret_cast<char*>(&entry.size), 4);

            char cmp[5] = { 0 };
            file.read(cmp, 4);
            entry.isPacked = (entry.name.substr(entry.name.length() - 4) == ".scp")
                && (entry.size > 12)
                && (strcmp(cmp, "CMP1") == 0);

            if (entry.isPacked) {
                file.read(reinterpret_cast<char*>(&entry.unpackedSize), 4);
            }
            else {
                entry.unpackedSize = entry.size;
            }

            entry.offset = baseOffset;
            baseOffset += entry.size;
            indexOffset += 0x20;

            directory.push_back(entry);
        }

        return true;
    }

public:
    PacExtractor(const std::string& filename) : file(filename, std::ios::binary) {
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }
        if (!readDirectory()) {
            std::cerr << "Failed to read PAC directory" << std::endl;
        }
    }

    void extractAll(const std::string& outputPath) {
        for (const auto& entry : directory) {
            std::string fullPath = outputPath + "/" + entry.name;

            // Create directories if they don't exist
            fs::path dirPath = fs::path(fullPath).parent_path();
            fs::create_directories(dirPath);

            std::ofstream outFile(fullPath, std::ios::binary);

            if (!outFile.is_open()) {
                std::cerr << "Failed to create file: " << fullPath << std::endl;
                continue;
            }

            file.seekg(entry.offset);
            std::vector<char> buffer(entry.size);
            file.read(buffer.data(), entry.size);
            outFile.write(buffer.data(), entry.size);

            outFile.close();
            std::cout << "Extracted: " << entry.name << " (Size: " << entry.size << " bytes)" << std::endl;
        }
    }
};

class PacPacker {
private:
    std::vector<Entry> directory;
    std::string inputPath;
    std::string outputFile;
    std::string originalPacFile;

    void readOriginalPac() {
        std::ifstream file(originalPacFile, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open original PAC file: " << originalPacFile << std::endl;
            return;
        }

        // Read signature and count
        uint32_t signature, count;
        file.read(reinterpret_cast<char*>(&signature), 4);
        file.read(reinterpret_cast<char*>(&count), 4);

        // Read directory entries
        for (uint32_t i = 0; i < count; ++i) {
            Entry entry;
            entry.originalHeader.resize(0x20);
            file.read(entry.originalHeader.data(), 0x20);

            entry.name = std::string(entry.originalHeader.data(), 16);
            entry.name = entry.name.c_str(); // Trim null terminators
            std::memcpy(&entry.size, entry.originalHeader.data() + 16, 4);
            std::memcpy(&entry.unpackedSize, entry.originalHeader.data() + 24, 4);

            directory.push_back(entry);
        }

        file.close();
    }

    void updateDirectoryWithNewFiles() {
        uint32_t offset = 8 + directory.size() * 0x20; // Start after header and directory

        for (auto& entry : directory) {
            std::string fullPath = inputPath + "/" + entry.name;
            if (fs::exists(fullPath)) {
                entry.size = fs::file_size(fullPath);
                entry.offset = offset;
                offset += entry.size;

                // Update size in original header
                std::memcpy(entry.originalHeader.data() + 16, &entry.size, 4);
            }
            else {
                std::cerr << "Warning: File not found: " << fullPath << std::endl;
            }
        }
    }

public:
    PacPacker(const std::string& inputDir, const std::string& outputFilename, const std::string& originalPac)
        : inputPath(inputDir), outputFile(outputFilename), originalPacFile(originalPac) {
        readOriginalPac();
        updateDirectoryWithNewFiles();
    }

    void pack() {
        std::ofstream file(outputFile, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to create output file: " << outputFile << std::endl;
            return;
        }

        // Write signature and count
        uint32_t signature = 0x31434150; // 'PAC1'
        uint32_t count = directory.size();
        file.write(reinterpret_cast<char*>(&signature), 4);
        file.write(reinterpret_cast<char*>(&count), 4);

        // Write directory
        for (const auto& entry : directory) {
            file.write(entry.originalHeader.data(), 0x20);
        }

        // Write file contents
        for (const auto& entry : directory) {
            std::string fullPath = inputPath + "/" + entry.name;
            std::ifstream inputFile(fullPath, std::ios::binary);
            if (inputFile.is_open()) {
                file << inputFile.rdbuf();
                inputFile.close();
            }
            else {
                std::cerr << "Failed to open input file: " << fullPath << std::endl;
            }
        }

        file.close();
        std::cout << "PAC file created: " << outputFile << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc == 3) {
        // Extract mode
        PacExtractor extractor(argv[1]);
        extractor.extractAll(argv[2]);
    }
    else if (argc == 4) {
        // Pack mode
        PacPacker packer(argv[1], argv[2], argv[3]);
        packer.pack();
    }
    else {
        std::cout << "Made by julixian 2025.01.01" << std::endl;
        std::cerr << "Usage:" << std::endl;
        std::cerr << "Extract: " << argv[0] << " <input.pac> <output_directory>" << std::endl;
        std::cerr << "Pack: " << argv[0] << " <input_directory> <output.pac> <original.pac>" << std::endl;
        return 1;
    }

    return 0;
}
