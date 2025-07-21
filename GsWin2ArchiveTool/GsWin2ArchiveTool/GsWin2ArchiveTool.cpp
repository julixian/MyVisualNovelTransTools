#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
namespace fs = std::filesystem;

#pragma pack(push, 1)
struct PakHeader {
    char magic[16];      // "GswSys PACK 2.0"
    uint32_t index_length;
    uint32_t index_entries;
    uint32_t data_offset;
};

struct PakEntry {
    char name[32];       // 0x20
    uint32_t offset;     // 0x4
    uint32_t length;     // 0x4
};

struct ScwHeader {
    char magic[16];      // "SCW for GswSys"
    uint32_t unk1;
    uint32_t unk2;
    uint32_t comprlen;
    uint32_t decomprlen;
    uint8_t pad[0xA8];
};
#pragma pack(pop)

extern "C" size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);

extern "C" size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);

class PakExtractor {
public:
    bool OpenPak(const std::string& pakPath) {
        m_file.open(pakPath, std::ios::binary);
        if (!m_file) return false;

        m_file.read((char*)&m_header, sizeof(PakHeader));
        if (strncmp(m_header.magic, "GswSys PACK 2.0", 11) != 0)
            return false;

        return true;
    }

    bool ExtractFiles(const std::string& outDir) {
        if (!m_file) return false;

        fs::create_directories(outDir);

        std::vector<uint8_t> compressedIndex(m_header.index_length);
        m_file.seekg(0x1C);
        m_file.read((char*)compressedIndex.data(), m_header.index_length);

        for (size_t i = 0; i < compressedIndex.size(); i++) {
            compressedIndex[i] ^= (i & 0xFF);
        }

        const size_t INDEX_ENTRY_SIZE = 0x28;
        size_t indexSize = m_header.index_entries * INDEX_ENTRY_SIZE;
        std::vector<uint8_t> indexData(indexSize);

        size_t decompSize = lzss_decompress(indexData.data(), indexSize,
            compressedIndex.data(), m_header.index_length);

        if (decompSize != indexSize) {
            std::cout << "Index decompression failed: expected " << indexSize
                << " bytes, got " << decompSize << " bytes\n";
            return false;
        }

        for (uint32_t i = 0; i < m_header.index_entries; i++) {
            PakEntry* entry = (PakEntry*)(indexData.data() + i * INDEX_ENTRY_SIZE);
            std::string outPath = outDir + "/" + entry->name;
            ExtractFile(*entry, outPath);
        }

        return true;
    }

private:
    bool ExtractFile(const PakEntry& entry, const std::string& outPath) {
        std::vector<uint8_t> fileData(entry.length);
        m_file.seekg(m_header.data_offset + entry.offset);
        m_file.read((char*)fileData.data(), entry.length);

        if (entry.length > sizeof(ScwHeader) &&
            memcmp(fileData.data(), "SCW for GswSys", 14) == 0) {
            return ExtractScwFile(fileData, outPath);
        }

        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) return false;
        outFile.write((char*)fileData.data(), entry.length);
        return true;
    }

    bool ExtractScwFile(std::vector<uint8_t>& fileData, const std::string& outPath) {
        ScwHeader* header = (ScwHeader*)fileData.data();
        uint8_t* data = fileData.data() + sizeof(ScwHeader);

        for (uint32_t i = 0; i < header->comprlen; i++)
            data[i] ^= (i & 0xff);

        std::vector<uint8_t> uncompressed(header->decomprlen);
        size_t decompSize = lzss_decompress(uncompressed.data(), header->decomprlen,
            data, header->comprlen);
        if (decompSize != header->decomprlen) return false;

        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) return false;
        outFile.write((char*)header, sizeof(ScwHeader));
        outFile.write((char*)uncompressed.data(), uncompressed.size());

        return true;
    }

private:
    std::ifstream m_file;
    PakHeader m_header;
};

class PakPacker {
public:
    PakPacker() {
        memset(&m_header, 0, sizeof(PakHeader));
        strcpy(m_header.magic, "GswSys PACK 2.0");
        m_header.data_offset = 0x5000;
    }
    std::vector<uint8_t> ProcessScwFile(const std::vector<uint8_t>& fileData) {
        if (fileData.size() < sizeof(ScwHeader)) {
            return fileData;
        }

        const ScwHeader* header = reinterpret_cast<const ScwHeader*>(fileData.data());

        if (strncmp(header->magic, "SCW for GswSys", 14) != 0) {
            return fileData;
        }

        std::vector<uint8_t> processedData = fileData;
        ScwHeader* newHeader = reinterpret_cast<ScwHeader*>(processedData.data());
        uint8_t* data = processedData.data() + sizeof(ScwHeader);

        size_t dataSize = fileData.size() - sizeof(ScwHeader);

        std::vector<uint8_t> compressedData = compress(
            std::vector<uint8_t>(data, data + dataSize));

        for (size_t i = 0; i < compressedData.size(); i++) {
            compressedData[i] ^= (i & 0xff);
        }

        newHeader->decomprlen = dataSize;
        newHeader->comprlen = compressedData.size();

        std::vector<uint8_t> result(sizeof(ScwHeader) + compressedData.size());
        memcpy(result.data(), newHeader, sizeof(ScwHeader));
        memcpy(result.data() + sizeof(ScwHeader),
            compressedData.data(), compressedData.size());

        return result;
    }

    bool PackFiles(const std::string& inputDir, const std::string& outputPath) {
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) return false;

        outFile.write((char*)&m_header, sizeof(PakHeader));

        std::vector<uint8_t> padding(m_header.data_offset - sizeof(PakHeader), 0);
        outFile.write((char*)padding.data(), padding.size());

        std::vector<PakEntry> entries;
        uint32_t currentOffset = 0;

        for (const auto& file : files) {
            PakEntry entry;
            memset(&entry, 0, sizeof(PakEntry));

            std::string relativePath = fs::relative(file, inputDir).string();
            std::cout << "Processing: " << relativePath << std::endl;
            strncpy(entry.name, relativePath.c_str(), sizeof(entry.name) - 1);

            std::ifstream inFile(file, std::ios::binary);
            if (!inFile) continue;

            inFile.seekg(0, std::ios::end);
            size_t fileSize = inFile.tellg();
            inFile.seekg(0);

            std::vector<uint8_t> fileData(fileSize);
            inFile.read((char*)fileData.data(), fileSize);

            std::vector<uint8_t> processedData = ProcessScwFile(fileData);

            entry.offset = currentOffset;
            entry.length = (processedData.size() + 3) & ~3;

            outFile.write((char*)processedData.data(), processedData.size());

            if (entry.length > processedData.size()) {
                std::vector<uint8_t> padding(entry.length - processedData.size(), 0);
                outFile.write((char*)padding.data(), padding.size());
            }

            entries.push_back(entry);
            currentOffset += entry.length;
        }

        std::vector<uint8_t> rawIndex;
        for (const auto& entry : entries) {
            const uint8_t* entryData = (const uint8_t*)&entry;
            rawIndex.insert(rawIndex.end(), entryData, entryData + sizeof(PakEntry));
        }

        std::vector<uint8_t> compressedIndex = compress(rawIndex);
        for (size_t j = 0; j < compressedIndex.size(); j++) {
            compressedIndex[j] ^= (j & 0xFF);
        }

        m_header.index_entries = entries.size();
        m_header.index_length = compressedIndex.size();

        outFile.seekp(0x1C);
        outFile.write((char*)compressedIndex.data(), compressedIndex.size());

        outFile.seekp(0);
        outFile.write((char*)&m_header, sizeof(PakHeader));

        return true;
    }

private:

    std::vector<uint8_t> compress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output((input.size() + 7) / 8 * 10);
        size_t compressedSize = lzss_compress(output.data(), output.size(), const_cast<uint8_t*>(input.data()), input.size());
        output.resize(compressedSize);
        return output;
    }

private:
    PakHeader m_header;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.07.21" << std::endl;
        std::cout << "Usage: " << argv[0] << " <mode> <input> <output>\n";
        std::cout << "Mode:\n";
        std::cout << "  -e : extract pak file to directory\n";
        std::cout << "  -p : create pak file from directory\n";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "-e") {
        PakExtractor extractor;
        if (!extractor.OpenPak(argv[2])) {
            std::cout << "Failed to open pak file\n";
            return 1;
        }

        if (!extractor.ExtractFiles(argv[3])) {
            std::cout << "Failed to extract files\n";
            return 1;
        }

        std::cout << "Extraction completed successfully\n";
    }
    else if (mode == "-p") {
        PakPacker packer;
        if (!packer.PackFiles(argv[2], argv[3])) {
            std::cout << "Failed to pack files\n";
            return 1;
        }

        std::cout << "Pack completed successfully\n";
    }
    else {
        std::cout << "Invalid mode\n";
        return 1;
    }

    return 0;
}
