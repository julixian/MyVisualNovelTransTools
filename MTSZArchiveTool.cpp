#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstring>
#include <zlib.h>
#include <algorithm>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct pak_header_t {
    char magic[48];
    uint32_t reserved[2];
    uint32_t index_entries;
    uint32_t zero;
};

struct pak_entry_t {
    char name[48];
    uint32_t offset0;
    uint32_t offset1;
    uint32_t length;
    uint32_t zero;
};
#pragma pack(pop)

std::vector<char> compress_data(const std::vector<char>& data) {
    uLongf compressedSize = compressBound(data.size());
    std::vector<char> compressedData(compressedSize);

    if (compress2(reinterpret_cast<Bytef*>(compressedData.data()), &compressedSize,
        reinterpret_cast<const Bytef*>(data.data()), data.size(),
        Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Compression failed");
    }

    compressedData.resize(compressedSize);
    return compressedData;
}

std::vector<char> decompress_data(const std::vector<char>& compressedData) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("inflateInit failed");
    }

    zs.next_in = (Bytef*)compressedData.data();
    zs.avail_in = compressedData.size();

    int ret;
    char outbuffer[32768];
    std::vector<char> decompressedData;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);

        if (decompressedData.size() < zs.total_out) {
            decompressedData.insert(decompressedData.end(), outbuffer, outbuffer + (zs.total_out - decompressedData.size()));
        }

    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Decompression failed");
    }

    return decompressedData;
}

void pack(const fs::path& inputFolder, const fs::path& fileListPath, const fs::path& outputFile) {
    std::vector<std::string> fileList;
    std::ifstream fileListStream(fileListPath);
    std::string fileName;
    while (std::getline(fileListStream, fileName)) {
        fileList.push_back(fileName);
    }

    std::vector<pak_entry_t> entries;
    std::vector<char> pakData;

    // Create header
    pak_header_t header = {};
    header.index_entries = fileList.size() + 1;  // 文件数加一

    // Calculate offsets and create entries
    uint32_t indexTableSize = header.index_entries * sizeof(pak_entry_t);
    uint32_t currentOffset = 0;
    for (const auto& fileName : fileList) {
        pak_entry_t entry = {};
        strncpy(entry.name, fileName.c_str(), sizeof(entry.name) - 1);
        entry.offset0 = entry.offset1 = currentOffset;

        fs::path filePath = inputFolder / fileName;
        entry.length = fs::file_size(filePath);

        currentOffset += entry.length;
        entries.push_back(entry);
    }

    // Add an extra all-zero entry
    pak_entry_t zeroEntry = {};
    entries.push_back(zeroEntry);

    // Write header
    pakData.insert(pakData.end(), reinterpret_cast<char*>(&header), reinterpret_cast<char*>(&header) + sizeof(header));

    // Write entries
    for (const auto& entry : entries) {
        pakData.insert(pakData.end(), reinterpret_cast<const char*>(&entry), reinterpret_cast<const char*>(&entry) + sizeof(entry));
    }

    // Write file contents
    for (const auto& fileName : fileList) {
        fs::path filePath = inputFolder / fileName;
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file: " << filePath << std::endl;
            return;
        }
        pakData.insert(pakData.end(), std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    // Compress the pak data
    std::vector<char> compressedData = compress_data(pakData);

    // Write compressed data to output file
    std::ofstream outFile(outputFile, std::ios::binary);
    outFile.write(compressedData.data(), compressedData.size());

    std::cout << "Pak file created and compressed successfully: " << outputFile << std::endl;
    std::cout << "Total entries: " << header.index_entries << " (including the extra zero entry)" << std::endl;
}

void unpack(const fs::path& inputFile, const fs::path& outputFolder) {
    // Read compressed data
    std::ifstream inFile(inputFile, std::ios::binary);
    std::vector<char> compressedData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());

    // Decompress data
    std::vector<char> pakData = decompress_data(compressedData);

    // Read header
    pak_header_t header;
    memcpy(&header, pakData.data(), sizeof(header));

    // Read entries
    std::vector<pak_entry_t> entries(header.index_entries);
    memcpy(entries.data(), pakData.data() + sizeof(header), header.index_entries * sizeof(pak_entry_t));

    // Calculate the start of file data
    uint32_t fileDataStart = sizeof(header) + header.index_entries * sizeof(pak_entry_t);

    // Create output folder if it doesn't exist
    fs::create_directories(outputFolder);

    // Open file list txt in the parent directory of the output folder
    fs::path fileListPath = outputFolder.parent_path() / "file_list.txt";
    std::ofstream fileList(fileListPath);

    // Extract files
    for (const auto& entry : entries) {
        if (entry.length == 0) continue; // Skip the extra zero entry

        fs::path filePath = outputFolder / entry.name;
        fs::create_directories(filePath.parent_path());

        // Use fileDataStart + entry.offset1 to get the correct position
        std::ofstream outFile(filePath, std::ios::binary);
        outFile.write(pakData.data() + fileDataStart + entry.offset1, entry.length);

        fileList << entry.name << std::endl;

        std::cout << "Extracted: " << entry.name << std::endl;
    }

    std::cout << "All files extracted to: " << outputFolder << std::endl;
    std::cout << "File list saved to: " << fileListPath << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << std::endl;
        std::cerr << "  Pack:   " << argv[0] << " pack <input_folder> <file_list.txt> <output_file.z>" << std::endl;
        std::cerr << "  Unpack: " << argv[0] << " unpack <input_file.z> <output_folder>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "pack") {
        if (argc != 5) {
            std::cerr << "Usage for pack: " << argv[0] << " pack <input_folder> <file_list.txt> <output_file.z>" << std::endl;
            return 1;
        }
        pack(argv[2], argv[3], argv[4]);
    }
    else if (mode == "unpack") {
        if (argc != 4) {
            std::cerr << "Usage for unpack: " << argv[0] << " unpack <input_file.z> <output_folder>" << std::endl;
            return 1;
        }
        unpack(argv[2], argv[3]);
    }
    else {
        std::cerr << "Invalid mode. Use 'pack' or 'unpack'." << std::endl;
        return 1;
    }

    return 0;
}
