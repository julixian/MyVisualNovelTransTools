#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

struct Entry {
    std::string name;
    uint32_t size;
    uint32_t offset;
    uint32_t relOffset;
    uint32_t indexOffset;  // 在目录区的偏移位置
};

class PD3Handler {
private:
    std::vector<Entry> m_entries;
    uint32_t m_indexCount;
    uint32_t m_fileCount;
    uint32_t m_totalSize;
    uint32_t m_baseOffset;
    std::vector<uint8_t> m_header; // 存储原始文件头和目录数据

    void ProcessContent(std::vector<uint8_t>& data) {
        for (auto& byte : data) {
            byte = (byte >> 4) | (byte << 4);
        }
    }

    bool NeedsProcess(const std::string& filename) {
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos) return false;
        std::string ext = filename.substr(pos);
        return (ext == ".def" || ext == ".dsf");
    }

    bool ReadOriginalStructure(std::ifstream& file) {
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&m_indexCount), 4);
        file.read(reinterpret_cast<char*>(&m_fileCount), 4);
        file.seekg(0xC);
        file.read(reinterpret_cast<char*>(&m_totalSize), 4);

        if (m_indexCount < m_fileCount || m_indexCount > 10000 || m_fileCount == 0) {
            return false;
        }

        m_baseOffset = 0x11C * m_indexCount + 0x18;
        m_header.resize(m_baseOffset);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(m_header.data()), m_baseOffset);

        uint32_t indexOffset = 0x18;
        for (uint32_t i = 0; i < m_indexCount; ++i) {
            file.seekg(indexOffset);

            // 读取整个目录项
            std::vector<char> entryData(0x11C);
            file.read(entryData.data(), 0x11C);

            if (entryData[0] != 0) {  // valid flag check
                Entry entry;
                entry.indexOffset = indexOffset;
                entry.name = std::string(entryData.data() + 0); // 直接从开头读取名称

                // 读取size和offset
                entry.size = *reinterpret_cast<uint32_t*>(entryData.data() + 0x108);
                entry.relOffset = *reinterpret_cast<uint32_t*>(entryData.data() + 0x10C);
                entry.offset = m_baseOffset + entry.relOffset;

                // 清理文件名中的空字符
                size_t nullPos = entry.name.find('\0');
                if (nullPos != std::string::npos) {
                    entry.name = entry.name.substr(0, nullPos);
                }

                m_entries.push_back(entry);
            }
            indexOffset += 0x11C;
        }

        return !m_entries.empty();
    }

public:
    bool ExtractAll(const std::string& pdFile, const std::string& outDir) {
        std::ifstream file(pdFile, std::ios::binary);
        if (!file || !ReadOriginalStructure(file)) {
            return false;
        }

        std::cout << "Found " << m_entries.size() << " files\n";

        for (const auto& entry : m_entries) {
            std::cout << "Extracting: " << entry.name << std::endl;

            std::vector<uint8_t> data(entry.size);
            file.seekg(entry.offset);
            file.read(reinterpret_cast<char*>(data.data()), entry.size);

            if (NeedsProcess(entry.name)) {
                ProcessContent(data);
            }

            fs::path outPath = fs::path(outDir) / entry.name;
            fs::create_directories(outPath.parent_path());

            std::ofstream outFile(outPath, std::ios::binary);
            if (!outFile) {
                std::cerr << "Failed to create: " << outPath << std::endl;
                continue;
            }
            outFile.write(reinterpret_cast<char*>(data.data()), data.size());
        }

        return true;
    }

    bool Repack(const std::string& pdFile, const std::string& inputDir, const std::string& outFile) {
        // 读取原始PD文件的结构
        std::ifstream origFile(pdFile, std::ios::binary);
        if (!origFile || !ReadOriginalStructure(origFile)) {
            return false;
        }

        // 创建新文件并写入原始头部和目录数据
        std::ofstream outPd(outFile, std::ios::binary);
        if (!outPd) {
            return false;
        }

        // 写入原始头部和目录数据
        outPd.write(reinterpret_cast<char*>(m_header.data()), m_header.size());

        // 处理每个文件
        uint32_t currentOffset = m_baseOffset;

        for (const auto& entry : m_entries) {
            fs::path inPath = fs::path(inputDir) / entry.name;

            // 如果文件不存在，保持原有数据
            if (!fs::exists(inPath)) {
                std::cout << "Keeping original: " << entry.name << std::endl;
                std::vector<uint8_t> origData(entry.size);
                origFile.seekg(entry.offset);
                origFile.read(reinterpret_cast<char*>(origData.data()), entry.size);
                outPd.seekp(currentOffset);
                outPd.write(reinterpret_cast<char*>(origData.data()), entry.size);
            }
            else {
                std::cout << "Updating: " << entry.name << std::endl;
                std::ifstream inFile(inPath, std::ios::binary);
                std::vector<uint8_t> data((std::istreambuf_iterator<char>(inFile)),
                    std::istreambuf_iterator<char>());

                if (NeedsProcess(entry.name)) {
                    ProcessContent(data);
                }

                // 写入新数据
                outPd.seekp(currentOffset);
                outPd.write(reinterpret_cast<char*>(data.data()), data.size());

                // 更新目录项信息
                uint32_t newSize = static_cast<uint32_t>(data.size());
                uint32_t relOffset = currentOffset - m_baseOffset;
                outPd.seekp(entry.indexOffset + 0x108);
                outPd.write(reinterpret_cast<char*>(&newSize), 4);
                outPd.write(reinterpret_cast<char*>(&relOffset), 4);
            }

            currentOffset += entry.size;
        }

        return true;
    }
};

void PrintUsage(const char* programName) {
    std::cout << "Made by julixian 2025.01.01" << std::endl;
    std::cout << "Usage:\n"
        << programName << " extract <input.pd> <output_dir>\n"
        << programName << " repack <original.pd> <input_dir> <output.pd>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    PD3Handler handler;

    if (command == "extract" && argc == 4) {
        if (!handler.ExtractAll(argv[2], argv[3])) {
            std::cerr << "Extraction failed\n";
            return 1;
        }
        std::cout << "Extraction completed successfully\n";
    }
    else if (command == "repack" && argc == 5) {
        if (!handler.Repack(argv[2], argv[3], argv[4])) {
            std::cerr << "Repacking failed\n";
            return 1;
        }
        std::cout << "Repacking completed successfully\n";
    }
    else {
        PrintUsage(argv[0]);
        return 1;
    }

    return 0;
}
