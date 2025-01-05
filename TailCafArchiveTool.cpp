#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

struct Entry {
    std::string name;
    uint32_t offset;
    uint32_t size;
    uint32_t indexOffset;
};

std::string readCString(std::ifstream& file, uint32_t offset) {
    file.seekg(offset);
    std::string result;
    char c;
    while (file.get(c) && c != '\0') {
        result += c;
    }
    return result;
}

bool extractCAF(const std::string& filename, const std::string& outputDir) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // Read header
    uint32_t signature;
    file.read(reinterpret_cast<char*>(&signature), 4);
    if (signature != 0x30464143) { // 'CAF0'
        std::cerr << "Invalid CAF file signature" << std::endl;
        return false;
    }

    uint32_t count, indexOffset, indexSize, namesOffset, namesSize;
    file.seekg(8);
    file.read(reinterpret_cast<char*>(&count), 4);
    file.read(reinterpret_cast<char*>(&indexOffset), 4);
    file.read(reinterpret_cast<char*>(&indexSize), 4);
    file.read(reinterpret_cast<char*>(&namesOffset), 4);
    file.read(reinterpret_cast<char*>(&namesSize), 4);

    // Read entries
    std::vector<Entry> entries;
    file.seekg(indexOffset);
    for (uint32_t i = 0; i < count; ++i) {
        Entry entry;
        uint32_t dirNameOffset, nameOffset;
        file.seekg(indexOffset + i * 0x14 + 4);
        file.read(reinterpret_cast<char*>(&dirNameOffset), 4);
        file.read(reinterpret_cast<char*>(&nameOffset), 4);
        file.read(reinterpret_cast<char*>(&entry.offset), 4);
        file.read(reinterpret_cast<char*>(&entry.size), 4);

        std::string dirName = (dirNameOffset >= 0) ? readCString(file, namesOffset + dirNameOffset) : "";
        std::string fileName = readCString(file, namesOffset + nameOffset);
        entry.name = dirName.empty() ? fileName : dirName + "/" + fileName;
        entry.offset += namesOffset + namesSize;

        entries.push_back(entry);
    }

    // Create output directory if it doesn't exist
    fs::create_directories(outputDir);

    // Extract files
    for (const auto& entry : entries) {
        fs::path outputPath = fs::path(outputDir) / entry.name;
        fs::create_directories(outputPath.parent_path());

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create file: " << outputPath << std::endl;
            continue;
        }

        file.seekg(entry.offset);
        std::vector<char> buffer(entry.size);
        file.read(buffer.data(), entry.size);
        outFile.write(buffer.data(), entry.size);

        std::cout << "Extracted: " << entry.name << std::endl;
    }

    return true;
}

bool updateCAF(const std::string& originalFile, const std::string& updateDir, const std::string& outputFile) {
    std::ifstream inFile(originalFile, std::ios::binary);
    if (!inFile) {
        std::cerr << "Failed to open original file: " << originalFile << std::endl;
        return false;
    }

    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create output file: " << outputFile << std::endl;
        return false;
    }

    // Read and write header
    uint32_t signature, count, indexOffset, indexSize, namesOffset, namesSize;
    inFile.read(reinterpret_cast<char*>(&signature), 4);
    inFile.seekg(8);
    inFile.read(reinterpret_cast<char*>(&count), 4);
    inFile.read(reinterpret_cast<char*>(&indexOffset), 4);
    inFile.read(reinterpret_cast<char*>(&indexSize), 4);
    inFile.read(reinterpret_cast<char*>(&namesOffset), 4);
    inFile.read(reinterpret_cast<char*>(&namesSize), 4);

    outFile.write(reinterpret_cast<char*>(&signature), 4);
    outFile.seekp(8);
    outFile.write(reinterpret_cast<char*>(&count), 4);
    outFile.write(reinterpret_cast<char*>(&indexOffset), 4);
    outFile.write(reinterpret_cast<char*>(&indexSize), 4);
    outFile.write(reinterpret_cast<char*>(&namesOffset), 4);
    outFile.write(reinterpret_cast<char*>(&namesSize), 4);

    // Copy the rest of the header
    inFile.seekg(0);
    outFile.seekp(0);
    std::vector<char> buffer(namesOffset + namesSize);
    inFile.read(buffer.data(), buffer.size());
    outFile.write(buffer.data(), buffer.size());

    // Read entries
    std::vector<Entry> entries;
    inFile.seekg(indexOffset);
    for (uint32_t i = 0; i < count; ++i) {
        Entry entry;
        uint32_t dirNameOffset, nameOffset;
        entry.indexOffset = indexOffset + i * 0x14;
        inFile.seekg(entry.indexOffset + 4);
        inFile.read(reinterpret_cast<char*>(&dirNameOffset), 4);
        inFile.read(reinterpret_cast<char*>(&nameOffset), 4);
        inFile.read(reinterpret_cast<char*>(&entry.offset), 4);
        inFile.read(reinterpret_cast<char*>(&entry.size), 4);

        std::string dirName = (dirNameOffset >= 0) ? readCString(inFile, namesOffset + dirNameOffset) : "";
        std::string fileName = readCString(inFile, namesOffset + nameOffset);
        entry.name = dirName.empty() ? fileName : dirName + "/" + fileName;
        entry.offset += namesOffset + namesSize;

        entries.push_back(entry);
    }

    // Update entries and write new data
    uint32_t currentOffset = namesOffset + namesSize;
    for (auto& entry : entries) {
        fs::path updatePath = fs::path(updateDir) / entry.name;
        if (fs::exists(updatePath) && fs::is_regular_file(updatePath)) {
            // Update with new file
            std::ifstream newFile(updatePath, std::ios::binary | std::ios::ate);
            std::streamsize newSize = newFile.tellg();
            newFile.seekg(0, std::ios::beg);

            entry.offset = currentOffset;
            entry.size = static_cast<uint32_t>(newSize);

            std::vector<char> newData(newSize);
            newFile.read(newData.data(), newSize);
            outFile.seekp(currentOffset);
            outFile.write(newData.data(), newSize);

            currentOffset += entry.size;
            std::cout << "Updated: " << entry.name << std::endl;
        }
        else {
            // Copy original data
            inFile.seekg(entry.offset);
            buffer.resize(entry.size);
            inFile.read(buffer.data(), entry.size);
            outFile.seekp(currentOffset);
            outFile.write(buffer.data(), entry.size);

            entry.offset = currentOffset;
            currentOffset += entry.size;
        }
    }

    // Update index
    for (const auto& entry : entries) {
        outFile.seekp(entry.indexOffset + 12);
        uint32_t relativeOffset = entry.offset - (namesOffset + namesSize);
        outFile.write(reinterpret_cast<const char*>(&relativeOffset), 4);
        outFile.write(reinterpret_cast<const char*>(&entry.size), 4);
    }

    std::cout << "CAF file updated successfully: " << outputFile << std::endl;
    return true;
}

void printUsage(const char* programName) {
    std::cout << "Made by julixian 2025.01.01" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Extract: " << programName << " extract <caf_file> <output_directory>" << std::endl;
    std::cout << "  Pack:  " << programName << " pack <original_caf_file> <pack_directory> <output_caf_file>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "extract") {
        if (argc != 4) {
            std::cerr << "Invalid number of arguments for extract command." << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        std::string cafFile = argv[2];
        std::string outputDir = argv[3];

        if (extractCAF(cafFile, outputDir)) {
            std::cout << "Extraction completed successfully. Files extracted to: " << outputDir << std::endl;
            return 0;
        }
        else {
            std::cerr << "Extraction failed." << std::endl;
            return 1;
        }
    }
    else if (command == "pack") {
        if (argc != 5) {
            std::cerr << "Invalid number of arguments for pack command." << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        std::string originalFile = argv[2];
        std::string updateDir = argv[3];
        std::string outputFile = argv[4];

        if (updateCAF(originalFile, updateDir, outputFile)) {
            std::cout << "CAF file pack completed successfully." << std::endl;
            return 0;
        }
        else {
            std::cerr << "CAF file pack failed." << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        return 1;
    }
}
