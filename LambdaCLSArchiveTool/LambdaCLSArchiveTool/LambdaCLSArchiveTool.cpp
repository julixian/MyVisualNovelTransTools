#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>

// Structure to hold file index information
struct FileEntry {
    char name[40];       // File name
    uint32_t offset;     // File data offset within the archive
    uint32_t size;       // File size
};

// Unpacking function
bool unpackClsDat(const std::filesystem::path& datPath, const std::filesystem::path& outputDir) {
    std::ifstream inputFile(datPath, std::ios::binary);
    if (!inputFile) {
        std::cerr << "Error: Could not open input file " << datPath << std::endl;
        return false;
    }

    char signature[13] = { 0 };
    inputFile.read(signature, 12);
    if (std::string(signature, 4) != "CLS_" || std::string(signature + 4, 8) != "FILELINK") {
        std::cerr << "Error: Invalid file format or not a CLS_FILELINK archive." << std::endl;
        return false;
    }

    inputFile.seekg(0x10);
    int32_t fileCount;
    inputFile.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    if (fileCount <= 0 || fileCount > 100000) { // Basic sanity check
        std::cerr << "Error: Invalid file count: " << fileCount << std::endl;
        return false;
    }

    inputFile.seekg(0x18);
    uint32_t indexOffset;
    inputFile.read(reinterpret_cast<char*>(&indexOffset), sizeof(indexOffset));

    std::cout << "Archive Info:" << std::endl;
    std::cout << "  File Count: " << fileCount << std::endl;
    std::cout << "  Index Table Offset: 0x" << std::hex << indexOffset << std::dec << std::endl;

    inputFile.seekg(indexOffset);
    std::vector<FileEntry> fileEntries;
    for (int i = 0; i < fileCount; ++i) {
        FileEntry entry;
        char entryBuffer[64];
        inputFile.read(entryBuffer, sizeof(entryBuffer));
        strncpy(entry.name, entryBuffer, sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
        memcpy(&entry.offset, entryBuffer + 0x2C, sizeof(uint32_t));
        memcpy(&entry.size, entryBuffer + 0x30, sizeof(uint32_t));
        fileEntries.push_back(entry);
    }

    try {
        std::filesystem::create_directories(outputDir);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: Could not create output directory " << outputDir << ": " << e.what() << std::endl;
        return false;
    }

    std::cout << "\nStarting to unpack to directory: " << outputDir << std::endl;
    std::vector<char> buffer(1024 * 1024); // 1MB buffer
    for (const auto& entry : fileEntries) {
        if (entry.name[0] == '\0' || entry.size == 0) continue;
        std::cout << "  Extracting: " << entry.name << " (" << entry.size << " bytes)" << std::endl;
        inputFile.seekg(entry.offset);
        std::filesystem::path outFilePath = outputDir / entry.name;
        std::ofstream outputFile(outFilePath, std::ios::binary);
        if (!outputFile) {
            std::cerr << "  Warning: Could not create file " << outFilePath << ", skipping." << std::endl;
            continue;
        }
        uint32_t remaining = entry.size;
        while (remaining > 0) {
            uint32_t toRead = std::min(remaining, (uint32_t)buffer.size());
            inputFile.read(buffer.data(), toRead);
            outputFile.write(buffer.data(), toRead);
            remaining -= toRead;
        }
    }
    std::cout << "\nUnpacking complete!" << std::endl;
    return true;
}

// Repacking function
bool repackClsDat(const std::filesystem::path& originalDatPath, const std::filesystem::path& inputDir, const std::filesystem::path& newDatPath) {
    // 1. Copy the original file to the new path
    try {
        std::filesystem::copy_file(originalDatPath, newDatPath, std::filesystem::copy_options::overwrite_existing);
        std::cout << "Copied " << originalDatPath << " to " << newDatPath << std::endl;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: Could not copy file: " << e.what() << std::endl;
        return false;
    }

    // 2. Open the newly created archive in read/write mode
    std::fstream repackerFile(newDatPath, std::ios::in | std::ios::out | std::ios::binary);
    if (!repackerFile) {
        std::cerr << "Error: Could not open the new archive in read/write mode " << newDatPath << std::endl;
        return false;
    }

    // 3. Read header info to locate the index table
    repackerFile.seekg(0x10);
    int32_t fileCount;
    repackerFile.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));

    repackerFile.seekg(0x18);
    uint32_t indexOffset;
    repackerFile.read(reinterpret_cast<char*>(&indexOffset), sizeof(indexOffset));

    std::cout << "\nStarting to update archive index..." << std::endl;

    // 4. Iterate through the index, find replacement files, and update
    for (int i = 0; i < fileCount; ++i) {
        uint32_t currentEntryOffset = indexOffset + (i * 64);

        // Read the current entry's file name
        char entryName[40] = { 0 };
        repackerFile.seekg(currentEntryOffset);
        repackerFile.read(entryName, sizeof(entryName) - 1);

        if (entryName[0] == '\0') {
            continue; // Skip empty entries
        }

        // Check if a replacement file exists
        std::filesystem::path replacementPath = inputDir / entryName;
        if (std::filesystem::exists(replacementPath)) {
            // Get the replacement file's size
            uint32_t newSize = std::filesystem::file_size(replacementPath);
            if (newSize == 0) {
                std::cout << "  - Warning: Replacement file " << entryName << " has a size of 0, skipping." << std::endl;
                continue;
            }

            // Go to the end of the archive to append new data
            repackerFile.seekp(0, std::ios::end);
            uint32_t currentEof = repackerFile.tellp();

            // Calculate padding for 0x20 (32-byte) alignment
            uint32_t alignment = 32;
            uint32_t padding = (alignment - (currentEof % alignment)) % alignment;

            // Write padding bytes
            if (padding > 0) {
                std::vector<char> padBuffer(padding, 0);
                repackerFile.write(padBuffer.data(), padding);
            }

            // The new file offset is after the padding
            uint32_t newOffset = currentEof + padding;

            std::cout << "  -> Updating: " << entryName << std::endl;
            std::cout << "     New Offset: 0x" << std::hex << newOffset << ", New Size: " << std::dec << newSize << " bytes" << std::endl;

            // Open the replacement file and append its content to the archive
            std::ifstream replacementFile(replacementPath, std::ios::binary);
            repackerFile << replacementFile.rdbuf();
            replacementFile.close();

            // 5. Go back to the index table entry and update offset and size
            repackerFile.seekp(currentEntryOffset + 0x2C); // Seek to offset field
            repackerFile.write(reinterpret_cast<const char*>(&newOffset), sizeof(newOffset));
            repackerFile.write(reinterpret_cast<const char*>(&newSize), sizeof(newSize));
        }
    }

    std::cout << "\nRepacking complete!" << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Made by julixian 2025.07.15" << std::endl;
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  To unpack: " << argv[0] << " unpack <input.dat> <output_dir>" << std::endl;
        std::cerr << "  To repack: " << argv[0] << " repack <original.dat> <mods_dir> <new.dat>" << std::endl;
        std::cerr << "\nExamples:" << std::endl;
        std::cerr << "  " << argv[0] << " unpack resource.dat extracted_files" << std::endl;
        std::cerr << "  " << argv[0] << " repack resource.dat mod_files new_resource.dat" << std::endl;
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "unpack" && argc == 4) {
        if (!unpackClsDat(argv[2], argv[3])) {
            return 1;
        }
    }
    else if (mode == "repack" && argc == 5) {
        if (!repackClsDat(argv[2], argv[3], argv[4])) {
            return 1;
        }
    }
    else {
        std::cerr << "Error: Invalid command or number of arguments." << std::endl;
        return 1;
    }

    return 0;
}
