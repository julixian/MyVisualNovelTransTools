#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct Entry {
    std::string name;
    uint32_t offset;
    uint32_t size;
};

bool extractDL1(const std::string& filename, const std::string& outputDir) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // Create output directory if it doesn't exist
    fs::create_directories(outputDir);

    // Check signature
    char signature[5] = { 0 };
    file.read(signature, 4);
    if (std::string(signature) != "DL1.") {
        std::cerr << "Invalid file signature" << std::endl;
        return false;
    }

    // Check version
    file.seekg(4);
    char version[3] = { 0 };
    file.read(version, 2);
    if (std::string(version) != "0\x1A") {
        std::cerr << "Invalid version" << std::endl;
        return false;
    }

    // Read file count
    file.seekg(8);
    uint16_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    // Read index offset
    file.seekg(0xA);
    uint32_t index_offset;
    file.read(reinterpret_cast<char*>(&index_offset), sizeof(index_offset));

    // Read file entries
    std::vector<Entry> entries;
    file.seekg(index_offset);
    for (int i = 0; i < count; ++i) {
        Entry entry;
        char name[13] = { 0 };
        file.read(name, 12);
        entry.name = std::string(name);
        file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));

        if (i == 0) {
            entry.offset = 0x10;
        }
        else {
            entry.offset = entries.back().offset + entries.back().size;
        }

        entries.push_back(entry);
    }

    // Extract files
    for (const auto& entry : entries) {
        fs::path outputPath = fs::path(outputDir) / entry.name;
        std::ofstream outfile(outputPath, std::ios::binary);
        if (!outfile) {
            std::cerr << "Failed to create file: " << outputPath << std::endl;
            continue;
        }

        file.seekg(entry.offset);
        std::vector<char> buffer(entry.size);
        file.read(buffer.data(), entry.size);
        outfile.write(buffer.data(), entry.size);

        std::cout << "Extracted: " << outputPath << std::endl;
    }

    return true;
}

bool updateDL1(const std::string& dl1Filename, const std::string& updateDir, const std::string& outputFilename) {
    std::ifstream inputFile(dl1Filename, std::ios::binary);
    if (!inputFile) {
        std::cerr << "Failed to open input file: " << dl1Filename << std::endl;
        return false;
    }

    std::ofstream outputFile(outputFilename, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Failed to create output file: " << outputFilename << std::endl;
        return false;
    }

    // Copy header (16 bytes)
    char header[16];
    inputFile.read(header, 16);
    outputFile.write(header, 16);

    // Read file count
    inputFile.seekg(8);
    uint16_t count;
    inputFile.read(reinterpret_cast<char*>(&count), sizeof(count));

    // Read index offset
    inputFile.seekg(0xA);
    uint32_t index_offset;
    inputFile.read(reinterpret_cast<char*>(&index_offset), sizeof(index_offset));

    // Read file entries
    std::vector<Entry> entries;
    inputFile.seekg(index_offset);
    for (int i = 0; i < count; ++i) {
        Entry entry;
        char name[13] = { 0 };
        inputFile.read(name, 12);
        entry.name = std::string(name);
        inputFile.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
        entries.push_back(entry);
    }

    // Update files and write new content
    uint32_t currentOffset = 0x10;  // Start of data section
    for (auto& entry : entries) {
        fs::path updatePath = fs::path(updateDir) / entry.name;

        if (fs::exists(updatePath)) {
            // Use updated file
            std::ifstream updateFile(updatePath, std::ios::binary);
            if (!updateFile) {
                std::cerr << "Failed to open update file: " << updatePath << std::endl;
                return false;
            }

            updateFile.seekg(0, std::ios::end);
            entry.size = static_cast<uint32_t>(updateFile.tellg());
            updateFile.seekg(0, std::ios::beg);

            std::vector<char> buffer(entry.size);
            updateFile.read(buffer.data(), entry.size);
            outputFile.write(buffer.data(), entry.size);

            std::cout << "Updated: " << entry.name << std::endl;
        }
        else {
            // Use original file content
            inputFile.seekg(entry.offset);
            std::vector<char> buffer(entry.size);
            inputFile.read(buffer.data(), entry.size);
            outputFile.write(buffer.data(), entry.size);

            std::cout << "Kept original: " << entry.name << std::endl;
        }

        entry.offset = currentOffset;
        currentOffset += entry.size;
    }

    // Write updated index
    index_offset = currentOffset;
    for (const auto& entry : entries) {
        outputFile.write(entry.name.c_str(), 12);
        outputFile.write(reinterpret_cast<const char*>(&entry.size), sizeof(entry.size));
    }

    // Update index offset in header
    outputFile.seekp(0xA);
    outputFile.write(reinterpret_cast<const char*>(&index_offset), sizeof(index_offset));

    std::cout << "DL1 file updated successfully." << std::endl;
    return true;
}

void printUsage(const char* programName) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  Extract: " << programName << " extract <DL1_file> <output_directory>" << std::endl;
    std::cout << "  Repack:  " << programName << " repack <input_orgi_DL1_file> <repack_directory> <output_DL1_file>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "extract" && argc == 4) {
        std::string inputFile = argv[2];
        std::string outputDir = argv[3];

        if (extractDL1(inputFile, outputDir)) {
            std::cout << "Extraction completed successfully." << std::endl;
        }
        else {
            std::cerr << "Extraction failed." << std::endl;
            return 1;
        }
    }
    else if (command == "repack" && argc == 5) {
        std::string inputFile = argv[2];
        std::string updateDir = argv[3];
        std::string outputFile = argv[4];

        if (updateDL1(inputFile, updateDir, outputFile)) {
            std::cout << "DL1 file update completed successfully." << std::endl;
        }
        else {
            std::cerr << "DL1 file update failed." << std::endl;
            return 1;
        }
    }
    else {
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
