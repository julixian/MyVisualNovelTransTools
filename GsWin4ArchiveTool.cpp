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
    char magic[16];      // "GsPack4 abc"
    char description[32]; // "GsPackFile4"
    uint16_t minor_version;
    uint16_t major_version;
    uint32_t index_length;
    uint32_t decode_key;
    uint32_t index_entries;
    uint32_t data_offset;
    uint32_t index_offset;
};

struct PakEntry {
    char name[64];       // 0x40
    uint32_t offset;     // 0x4
    uint32_t length;     // 0x4
};

struct ScwHeader {
    char magic[16];      // "Scw4.x"
    uint16_t minor_version;
    uint16_t major_version;
    int32_t is_compr;
    uint32_t uncomprlen;
    uint32_t comprlen;
    uint32_t always_1;
    uint32_t instruction_table_entries;
    uint32_t string_table_entries;
    uint32_t unknown_table_entries;
    uint32_t instruction_data_length;
    uint32_t string_data_length;
    uint32_t unknown_data_length;
    uint8_t pad[0x188];
};
#pragma pack(pop)

size_t lzss_decompress(uint8_t* uncompr, size_t uncomprlen,
    const uint8_t* compr, size_t comprlen)
{
    size_t act_uncomprlen = 0;
    size_t curbyte = 0;
    size_t nCurWindowByte = 0x0fee;
    uint8_t win[4096];
    memset(win, 0, sizeof(win));

    while (curbyte < comprlen) {
        uint8_t bitmap = compr[curbyte++];
        for (int curbit = 0; curbit < 8; curbit++) {
            if (bitmap & (1 << curbit)) {
                if (curbyte >= comprlen) break;
                uint8_t data = compr[curbyte++];

                if (act_uncomprlen >= uncomprlen) break;
                uncompr[act_uncomprlen++] = data;
                win[nCurWindowByte++] = data;
                nCurWindowByte &= 0xfff;
            }
            else {
                if (curbyte + 1 >= comprlen) break;

                uint32_t win_offset = compr[curbyte++];
                uint32_t copy_bytes = compr[curbyte++];
                win_offset |= (copy_bytes >> 4) << 8;
                copy_bytes = (copy_bytes & 0x0f) + 3;

                for (uint32_t i = 0; i < copy_bytes; i++) {
                    uint8_t data = win[(win_offset + i) & 0xfff];
                    if (act_uncomprlen >= uncomprlen) break;

                    uncompr[act_uncomprlen++] = data;
                    win[nCurWindowByte++] = data;
                    nCurWindowByte &= 0xfff;
                }
            }
        }
    }
    return act_uncomprlen;
}

class PakExtractor {
public:
    bool OpenPak(const std::string& pakPath) {
        m_file.open(pakPath, std::ios::binary);
        if (!m_file) return false;

        m_file.read((char*)&m_header, sizeof(PakHeader));
        if (strncmp(m_header.magic, "GsPack4 abc", 11) != 0)
            return false;

        return true;
    }

    bool ExtractFiles(const std::string& outDir) {
        if (!m_file) return false;

        fs::create_directories(outDir);

        std::vector<uint8_t> compressedIndex(m_header.index_length);
        m_file.seekg(m_header.index_offset);
        m_file.read((char*)compressedIndex.data(), m_header.index_length);

        if (m_header.decode_key) {
            for (size_t i = 0; i < compressedIndex.size(); i++)
                compressedIndex[i] ^= (i & m_header.decode_key);
        }

        const size_t INDEX_ENTRY_SIZE = 0x48;
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
            memcmp(fileData.data(), "Scw4.x", 6) == 0) {
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

        if (header->is_compr == -1) {
            for (uint32_t i = 0; i < header->comprlen; i++)
                data[i] ^= i & 0xff;

            std::vector<uint8_t> uncompressed(header->uncomprlen);
            size_t decompSize = lzss_decompress(uncompressed.data(), header->uncomprlen,
                data, header->comprlen);
            if (decompSize != header->uncomprlen) return false;

            header->is_compr = 0;
            header->comprlen = header->uncomprlen;

            std::ofstream outFile(outPath, std::ios::binary);
            if (!outFile) return false;
            outFile.write((char*)header, sizeof(ScwHeader));
            outFile.write((char*)uncompressed.data(), uncompressed.size());
        }
        else {
            std::ofstream outFile(outPath, std::ios::binary);
            if (!outFile) return false;
            outFile.write((char*)fileData.data(), fileData.size());
        }
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
        strcpy(m_header.magic, "GsPack4 abc");
        strcpy(m_header.description, "GsPackFile4");
        m_header.major_version = 4;
        m_header.minor_version = 0;
        m_header.decode_key = 0x00;
        m_header.data_offset = 0x800;
    }
    std::vector<uint8_t> ProcessScwFile(const std::vector<uint8_t>& fileData) {
        if (fileData.size() < sizeof(ScwHeader)) {
            return fileData;
        }

        const ScwHeader* header = reinterpret_cast<const ScwHeader*>(fileData.data());

        if (strncmp(header->magic, "Scw4.x", 6) != 0) {
            return fileData;
        }

        std::vector<uint8_t> processedData = fileData;
        ScwHeader* newHeader = reinterpret_cast<ScwHeader*>(processedData.data());
        uint8_t* data = processedData.data() + sizeof(ScwHeader);

        if (newHeader->is_compr == 0) {
            size_t dataSize = fileData.size() - sizeof(ScwHeader);

            std::vector<uint8_t> compressedData = compress(
                std::vector<uint8_t>(data, data + dataSize));

            for (size_t i = 0; i < compressedData.size(); i++) {
                compressedData[i] ^= i & 0xff;
            }

            newHeader->is_compr = -1;
            newHeader->uncomprlen = dataSize;
            newHeader->comprlen = compressedData.size();

            std::vector<uint8_t> result(sizeof(ScwHeader) + compressedData.size());
            memcpy(result.data(), newHeader, sizeof(ScwHeader));
            memcpy(result.data() + sizeof(ScwHeader),
                compressedData.data(), compressedData.size());

            return result;
        }

        return processedData;
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

        std::vector<uint8_t> padding(0x800 - sizeof(PakHeader), 0);
        outFile.write((char*)padding.data(), padding.size());

        std::vector<PakEntry> entries;
        uint32_t currentOffset = 0;

        for (const auto& file : files) {
            PakEntry entry;
            memset(&entry, 0, sizeof(PakEntry));

            std::string relativePath = fs::relative(file, inputDir).string();
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

        m_header.index_entries = entries.size();
        m_header.index_length = compressedIndex.size();
        m_header.index_offset = outFile.tellp();

        outFile.write((char*)compressedIndex.data(), compressedIndex.size());

        outFile.seekp(0);
        outFile.write((char*)&m_header, sizeof(PakHeader));

        return true;
    }

private:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        for (size_t i = 0; i < input.size(); i += 8) {
            output.push_back(0xFF);
            for (size_t j = 0; j < 8 && i + j < input.size(); ++j) {
                output.push_back(input[i + j]);
            }
        }
        return output;
    }

private:
    PakHeader m_header;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.03.12" << std::endl;
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
