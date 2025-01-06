#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct Entry {
    uint32_t unpackedSize;
    uint32_t size;
    uint32_t offset;
    std::string name;
    uint32_t nameOffset;
};

class TlzExtractor {
public:
    bool openArchive(const std::string& filename) {
        file.open(filename, std::ios::binary);
        return file.is_open();
    }

    bool extractFiles(const std::string& outputDir) {
        if (!file.is_open()) return false;

        uint32_t signature;
        file.read(reinterpret_cast<char*>(&signature), sizeof(signature));
        if (signature != 0x315A4C54) { // 'TLZ1'
            std::cerr << "Invalid TLZ file signature" << std::endl;
            return false;
        }

        uint32_t indexOffset;
        file.seekg(4, std::ios::beg);
        file.read(reinterpret_cast<char*>(&indexOffset), sizeof(indexOffset));

        int32_t count;
        file.seekg(0xC, std::ios::beg);
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        if (count <= 0 || count > 10000) { // Sanity check
            std::cerr << "Invalid file count" << std::endl;
            return false;
        }

        std::vector<Entry> entries;
        file.seekg(indexOffset, std::ios::beg);

        for (int i = 0; i < count; ++i) {
            Entry entry;
            file.read(reinterpret_cast<char*>(&entry.unpackedSize), sizeof(entry.unpackedSize));
            file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
            file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));

            uint32_t nameLength;
            file.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
            if (nameLength == 0 || nameLength > 0x100) {
                std::cerr << "Invalid name length for entry " << i << std::endl;
                return false;
            }

            char* nameBuf = new char[nameLength + 1];
            file.read(nameBuf, nameLength);
            nameBuf[nameLength] = '\0';
            entry.name = std::string(nameBuf);
            delete[] nameBuf;

            entries.push_back(entry);
        }

        fs::create_directories(outputDir);

        for (const auto& entry : entries) {
            std::string outputPath = outputDir + "/" + entry.name;
            std::ofstream outFile(outputPath, std::ios::binary);
            if (!outFile) {
                std::cerr << "Failed to create output file: " << outputPath << std::endl;
                continue;
            }

            file.seekg(entry.offset, std::ios::beg);
            std::vector<char> buffer(entry.size);
            file.read(buffer.data(), entry.size);
            outFile.write(buffer.data(), entry.size);
            outFile.close();

            std::cout << "Extracted: " << entry.name << std::endl;
        }

        return true;
    }

    ~TlzExtractor() {
        if (file.is_open()) file.close();
    }

private:
    std::ifstream file;
};

class TlzUpdater {
public:
    bool openArchive(const std::string& filename) {
        archiveName = filename;
        file.open(filename, std::ios::binary | std::ios::in | std::ios::out);
        return file.is_open();
    }

    bool updateArchive(const std::string& updateDir) {
        if (!file.is_open()) return false;

        uint32_t signature;
        file.read(reinterpret_cast<char*>(&signature), sizeof(signature));
        if (signature != 0x315A4C54) { // 'TLZ1'
            std::cerr << "Invalid TLZ file signature" << std::endl;
            return false;
        }

        uint32_t indexOffset;
        file.seekg(4, std::ios::beg);
        file.read(reinterpret_cast<char*>(&indexOffset), sizeof(indexOffset));

        int32_t count;
        file.seekg(0xC, std::ios::beg);
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        if (count <= 0 || count > 10000) { // Sanity check
            std::cerr << "Invalid file count" << std::endl;
            return false;
        }

        std::vector<Entry> entries;
        file.seekg(indexOffset, std::ios::beg);

        // Read original entries
        for (int i = 0; i < count; ++i) {
            Entry entry;
            entry.nameOffset = file.tellg();
            file.read(reinterpret_cast<char*>(&entry.unpackedSize), sizeof(entry.unpackedSize));
            file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
            file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));

            uint32_t nameLength;
            file.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
            if (nameLength == 0 || nameLength > 0x100) {
                std::cerr << "Invalid name length for entry " << i << std::endl;
                return false;
            }

            char* nameBuf = new char[nameLength + 1];
            file.read(nameBuf, nameLength);
            nameBuf[nameLength] = '\0';
            entry.name = std::string(nameBuf);
            delete[] nameBuf;

            entries.push_back(entry);
        }

        // Find the end of the last file
        uint32_t endOfLastFile = 0;
        for (const auto& entry : entries) {
            endOfLastFile = std::max(endOfLastFile, entry.offset + entry.size);
        }

        // Update entries and file content
        for (auto& entry : entries) {
            std::string updatePath = updateDir + "/" + entry.name;
            if (fs::exists(updatePath)) {
                // Update file content
                std::ifstream newFile(updatePath, std::ios::binary);
                if (!newFile) {
                    std::cerr << "Failed to open pack file: " << updatePath << std::endl;
                    continue;
                }

                newFile.seekg(0, std::ios::end);
                uint32_t newSize = newFile.tellg();
                newFile.seekg(0, std::ios::beg);

                std::vector<char> buffer(newSize);
                newFile.read(buffer.data(), newSize);

                if (true) {//safe
                    // Append to the end of the file
                    file.seekp(0, std::ios::end);
                    entry.offset = file.tellp();
                }
                else {
                    // Use the original position
                    file.seekp(entry.offset, std::ios::beg);
                }

                file.write(buffer.data(), newSize);

                // Update entry information
                entry.size = newSize;
                entry.unpackedSize = newSize; // Assuming uncompressed for simplicity

                // Update entry in the index
                file.seekp(entry.nameOffset, std::ios::beg);
                file.write(reinterpret_cast<const char*>(&entry.unpackedSize), sizeof(entry.unpackedSize));
                file.write(reinterpret_cast<const char*>(&entry.size), sizeof(entry.size));
                file.write(reinterpret_cast<const char*>(&entry.offset), sizeof(entry.offset));

                std::cout << "Packed: " << entry.name << std::endl;

                // Update endOfLastFile if necessary
                endOfLastFile = std::max(endOfLastFile, entry.offset + entry.size);
            }
        }

        // Truncate the file if necessary
        file.seekp(endOfLastFile, std::ios::beg);
        file.close();
        fs::resize_file(archiveName, endOfLastFile);

        std::cout << "Archive packed successfully" << std::endl;
        return true;
    }

    ~TlzUpdater() {
        if (file.is_open()) file.close();
    }

private:
    std::fstream file;
    std::string archiveName;
};

void printUsage(const char* programName) {
    std::cerr << "Made by julixian 2025.01.06" << std::endl;
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  Extract: " << programName << " extract <input.tlz> <output_directory>" << std::endl;
    std::cerr << "  Repack:  " << programName << " pack <orgi_input.tlz> <update_directory> <output.tlz>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "extract") {
        if (argc != 4) {
            printUsage(argv[0]);
            return 1;
        }

        TlzExtractor extractor;
        if (!extractor.openArchive(argv[2])) {
            std::cerr << "Failed to open archive: " << argv[2] << std::endl;
            return 1;
        }

        if (!extractor.extractFiles(argv[3])) {
            std::cerr << "Failed to extract files" << std::endl;
            return 1;
        }

        std::cout << "Extraction completed successfully" << std::endl;
    }
    else if (command == "pack") {
        if (argc != 5) {
            printUsage(argv[0]);
            return 1;
        }

        // 创建输入文件的副本作为输出文件
        std::filesystem::copy_file(argv[2], argv[4], std::filesystem::copy_options::overwrite_existing);

        TlzUpdater updater;
        if (!updater.openArchive(argv[4])) {
            std::cerr << "Failed to open archive: " << argv[4] << std::endl;
            return 1;
        }

        if (!updater.updateArchive(argv[3])) {
            std::cerr << "Failed to pack archive" << std::endl;
            return 1;
        }

        std::cout << "Archive packed and saved as: " << argv[4] << std::endl;
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
