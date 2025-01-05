#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <map>
#include <algorithm>

namespace fs = std::filesystem;

struct TcdSection {
    std::string Extension;
    uint32_t DataSize;
    uint32_t IndexOffset;
    int32_t DirCount;
    int32_t DirNameLength;
    int32_t FileCount;
    int32_t FileNameLength;
    int32_t DirNamesSize;
    int32_t FileNamesSize;
};

struct TcdDirEntry {
    int32_t FileCount;
    int32_t NamesOffset;
    int32_t FirstIndex;
};

struct TcdEntry {
    std::string Name;
    uint32_t Offset;
    uint32_t Size;
    int32_t Index;
};

class TcdIndexReader {
protected:
    std::ifstream m_input;
    int m_section_count;
    int Count;
    std::vector<std::string> Extensions = { ".TCT", ".TSF", ".SPD", ".OGG", ".WAV" };

    TcdIndexReader(const std::string& filename, int section_count)
        : m_input(filename, std::ios::binary), m_section_count(section_count) {
        m_input.seekg(4);
        m_input.read(reinterpret_cast<char*>(&Count), sizeof(int32_t));
    }

    void DecryptNames(std::vector<char>& buffer, char key) {
        for (char& c : buffer) {
            c -= key;
        }
    }

    virtual TcdSection ReadSection(int number) = 0;
    virtual std::string GetName(const std::vector<char>& names, int name_length, int& offset) = 0;

public:
    virtual ~TcdIndexReader() {
        if (m_input.is_open()) {
            m_input.close();
        }
    }

    std::vector<TcdEntry> ReadIndex() {
        std::vector<TcdEntry> list;
        std::vector<TcdSection> sections = ReadSections(m_section_count);
        list.reserve(Count);

        for (const auto& section : sections) {
            m_input.seekg(section.IndexOffset);
            std::vector<char> dir_names(section.DirNamesSize);
            m_input.read(dir_names.data(), section.DirNamesSize);
            char section_key = dir_names[section.DirNameLength - 1];
            DecryptNames(dir_names, section_key);

            std::vector<TcdDirEntry> dirs(section.DirCount);
            for (auto& dir : dirs) {
                m_input.read(reinterpret_cast<char*>(&dir), sizeof(TcdDirEntry));
                m_input.seekg(4, std::ios::cur); // Skip 4 bytes
            }

            std::vector<char> file_names(section.FileNamesSize);
            m_input.read(file_names.data(), section.FileNamesSize);
            DecryptNames(file_names, section_key);

            std::vector<uint32_t> offsets(section.FileCount + 1);
            m_input.read(reinterpret_cast<char*>(offsets.data()), offsets.size() * sizeof(uint32_t));

            int dir_name_offset = 0;
            for (const auto& dir : dirs) {
                std::string dir_name = GetName(dir_names, section.DirNameLength, dir_name_offset);
                int index = dir.FirstIndex;
                int name_offset = dir.NamesOffset;
                for (int i = 0; i < dir.FileCount; ++i) {
                    std::string name = GetName(file_names, section.FileNameLength, name_offset);
                    name = (fs::path(dir_name) / name).string();
                    name = fs::path(name).replace_extension(section.Extension).string();

                    TcdEntry entry;
                    entry.Name = name;
                    entry.Offset = offsets[index];
                    entry.Size = offsets[index + 1] - offsets[index];
                    entry.Index = index;
                    ++index;
                    list.push_back(entry);
                }
            }
        }
        return list;
    }

private:
    std::vector<TcdSection> ReadSections(int count) {
        std::vector<TcdSection> sections;
        uint32_t current_offset = 8;
        for (int i = 0; i < count; ++i) {
            m_input.seekg(current_offset);
            auto section = ReadSection(i);
            if (section.DataSize != 0) {
                sections.push_back(section);
            }
            current_offset += 0x20;
        }
        return sections;
    }
};

class TcdReaderV2 : public TcdIndexReader {
public:
    TcdReaderV2(const std::string& filename) : TcdIndexReader(filename, 4) {}

protected:
    TcdSection ReadSection(int number) override {
        TcdSection section;
        m_input.read(reinterpret_cast<char*>(&section.DataSize), sizeof(uint32_t));
        if (section.DataSize == 0) return section;

        section.Extension = Extensions[number];
        m_input.read(reinterpret_cast<char*>(&section.FileCount), sizeof(int32_t));
        m_input.read(reinterpret_cast<char*>(&section.DirCount), sizeof(int32_t));
        m_input.read(reinterpret_cast<char*>(&section.IndexOffset), sizeof(uint32_t));
        m_input.read(reinterpret_cast<char*>(&section.DirNameLength), sizeof(int32_t));
        m_input.read(reinterpret_cast<char*>(&section.FileNameLength), sizeof(int32_t));
        section.DirNamesSize = section.DirNameLength;
        section.FileNamesSize = section.FileNameLength;
        return section;
    }

    std::string GetName(const std::vector<char>& names, int name_length, int& offset) override {
        auto name_end = std::find(names.begin() + offset, names.end(), '\0');
        std::string name(names.begin() + offset, name_end);
        offset += name.length() + 1;
        return name;
    }
};

class TcdUpdater {
private:
    struct SectionInfo {
        uint32_t startOffset;
        TcdSection section;
        std::vector<TcdDirEntry> dirs;
        std::vector<uint32_t> offsets;
    };

    std::string m_originalTcd;
    std::string m_updateDir;
    std::string m_outputTcd;
    std::vector<TcdEntry> m_entries;
    std::map<std::string, fs::path> m_updateFiles;
    std::vector<SectionInfo> m_sections;
    std::vector<std::string> Extensions = { ".TCT", ".TSF", ".SPD", ".OGG", ".WAV" };

public:
    TcdUpdater(const std::string& originalTcd, const std::string& updateDir, const std::string& outputTcd)
        : m_originalTcd(originalTcd), m_updateDir(updateDir), m_outputTcd(outputTcd) {
    }

    bool Update() {
        // 读取原始TCD文件的索引和目录信息
        TcdReaderV2 reader(m_originalTcd);
        m_entries = reader.ReadIndex();

        if (!ReadOriginalTcd())
            return false;

        // 扫描更新目录中的文件
        ScanUpdateDirectory();

        // 复制原始TCD文件作为基础
        if (!fs::copy_file(m_originalTcd, m_outputTcd, fs::copy_options::overwrite_existing)) {
            std::cerr << "Failed to create output TCD file." << std::endl;
            return false;
        }

        // 打开输出文件进行更新
        std::fstream outFile(m_outputTcd, std::ios::in | std::ios::out | std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to open output TCD file." << std::endl;
            return false;
        }

        // 获取文件末尾位置作为新数据的起始位置
        outFile.seekg(0, std::ios::end);
        uint32_t currentOffset = static_cast<uint32_t>(outFile.tellg());

        return UpdateFileContents(outFile, currentOffset);
    }

private:
    bool ReadOriginalTcd() {
        std::ifstream file(m_originalTcd, std::ios::binary);
        if (!file) return false;

        // 读取文件头
        uint32_t signature;
        file.read(reinterpret_cast<char*>(&signature), sizeof(signature));
        int32_t totalFiles;
        file.read(reinterpret_cast<char*>(&totalFiles), sizeof(totalFiles));

        // 读取每个区段的信息
        uint32_t currentOffset = 8;
        for (int i = 0; i < 4; i++) {
            file.seekg(currentOffset);

            SectionInfo info;
            info.startOffset = currentOffset;

            // 读取区段头
            uint32_t dataSize;
            file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
            if (dataSize == 0) {
                currentOffset += 0x20;
                continue;
            }

            info.section.DataSize = dataSize;
            info.section.Extension = Extensions[i];
            file.read(reinterpret_cast<char*>(&info.section.FileCount), sizeof(int32_t));
            file.read(reinterpret_cast<char*>(&info.section.DirCount), sizeof(int32_t));
            file.read(reinterpret_cast<char*>(&info.section.IndexOffset), sizeof(uint32_t));
            file.read(reinterpret_cast<char*>(&info.section.DirNameLength), sizeof(int32_t));
            file.read(reinterpret_cast<char*>(&info.section.FileNameLength), sizeof(int32_t));

            // 读取目录项
            file.seekg(info.section.IndexOffset + info.section.DirNameLength);
            info.dirs.resize(info.section.DirCount);
            for (auto& dir : info.dirs) {
                file.read(reinterpret_cast<char*>(&dir), sizeof(TcdDirEntry));
                file.seekg(4, std::ios::cur);
            }

            // 读取偏移表
            file.seekg(info.section.IndexOffset +
                info.section.DirNameLength +
                (info.section.DirCount * 16) +
                info.section.FileNameLength);

            info.offsets.resize(info.section.FileCount + 1);
            file.read(reinterpret_cast<char*>(info.offsets.data()),
                info.offsets.size() * sizeof(uint32_t));

            m_sections.push_back(info);
            currentOffset += 0x20;
        }

        return true;
    }

    void ScanUpdateDirectory() {
        for (const auto& entry : fs::recursive_directory_iterator(m_updateDir)) {
            if (entry.is_regular_file()) {
                std::string relativePath = fs::relative(entry.path(), m_updateDir).string();
                m_updateFiles[relativePath] = entry.path();
            }
        }
    }

    bool UpdateFileContents(std::fstream& outFile, uint32_t& currentOffset) {
        std::vector<char> buffer;
        // 记录每个区段的文件更新信息
        struct UpdateInfo {
            uint32_t oldOffset;
            uint32_t newOffset;
            uint32_t oldSize;
            uint32_t newSize;
        };
        // <section_index, <file_index, update_info>>
        std::map<size_t, std::map<size_t, UpdateInfo>> sectionUpdates;

        // 首先收集所有需要更新的文件信息
        for (size_t i = 0; i < m_entries.size(); ++i) {
            auto& entry = m_entries[i];
            auto it = m_updateFiles.find(entry.Name);

            if (it != m_updateFiles.end()) {
                std::ifstream newFile(it->second, std::ios::binary);
                if (!newFile) continue;

                // 获取新文件大小
                newFile.seekg(0, std::ios::end);
                uint32_t newSize = static_cast<uint32_t>(newFile.tellg());
                newFile.seekg(0);

                // 将新文件追加到输出文件末尾
                buffer.resize(newSize);
                newFile.read(buffer.data(), newSize);
                outFile.seekp(currentOffset);
                outFile.write(buffer.data(), newSize);

                // 找到文件所属的区段
                for (size_t secIdx = 0; secIdx < m_sections.size(); ++secIdx) {
                    auto& section = m_sections[secIdx];
                    for (size_t fileIdx = 0; fileIdx < section.offsets.size() - 1; ++fileIdx) {
                        if (section.offsets[fileIdx] == entry.Offset) {
                            UpdateInfo info;
                            info.oldOffset = entry.Offset;
                            info.newOffset = currentOffset;
                            info.oldSize = section.offsets[fileIdx + 1] - section.offsets[fileIdx];
                            info.newSize = newSize;
                            sectionUpdates[secIdx][fileIdx] = info;
                            break;
                        }
                    }
                }

                currentOffset += newSize;
                std::cout << "Updated: " << entry.Name << std::endl;
            }
        }

        // 更新各个区段
        for (size_t secIdx = 0; secIdx < m_sections.size(); ++secIdx) {
            auto sectionIt = sectionUpdates.find(secIdx);
            if (sectionIt == sectionUpdates.end()) continue;

            auto& section = m_sections[secIdx];
            int32_t sizeDelta = 0; // 区段大小的变化量

            // 更新偏移表
            for (const auto& [fileIdx, info] : sectionIt->second) {
                section.offsets[fileIdx] = info.newOffset;

                // 更新下一个偏移
                if (fileIdx + 1 < section.offsets.size()) {
                    section.offsets[fileIdx + 1] = info.newOffset + info.newSize;
                }

                sizeDelta += (info.newSize - info.oldSize);
            }

            // 更新区段的 DataSize
            uint32_t newDataSize = section.section.DataSize + sizeDelta;
            outFile.seekp(section.startOffset);
            outFile.write(reinterpret_cast<const char*>(&newDataSize), sizeof(uint32_t));

            // 写回更新后的偏移表
            uint32_t offsetTablePos = section.section.IndexOffset +
                section.section.DirNameLength +
                (section.section.DirCount * 16) +
                section.section.FileNameLength;

            outFile.seekp(offsetTablePos);
            outFile.write(reinterpret_cast<const char*>(section.offsets.data()),
                section.offsets.size() * sizeof(uint32_t));
        }

        return true;
    }

};

bool ExtractFiles(const std::string& tcdPath, const std::string& outputDir) {
    TcdReaderV2 reader(tcdPath);
    auto entries = reader.ReadIndex();

    std::ifstream tcdFile(tcdPath, std::ios::binary);
    if (!tcdFile) {
        std::cerr << "Failed to open TCD file." << std::endl;
        return false;
    }

    for (const auto& entry : entries) {
        fs::path outputPath = fs::path(outputDir) / entry.Name;
        fs::create_directories(outputPath.parent_path());

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create file: " << entry.Name << std::endl;
            continue;
        }

        tcdFile.seekg(entry.Offset);
        std::vector<char> buffer(entry.Size);
        tcdFile.read(buffer.data(), entry.Size);
        outFile.write(buffer.data(), entry.Size);

        std::cout << "Extracted: " << entry.Name << std::endl;
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Made by julixian 2025.01.01" << std::endl;
        std::cerr << "Usage for extract: " << argv[0] << " extract <tcd_file> <output_directory>" << std::endl;
        std::cerr << "Usage for update: " << argv[0] << " update <original_tcd> <update_directory> <output_tcd>" << std::endl;
        return 1;
    }

    std::string operation = argv[1];

    if (operation == "extract") {  // 解包操作
        if (argc != 4) {
            std::cerr << "Usage for extract: " << argv[0] << " -e <tcd_file> <output_directory>" << std::endl;
            return 1;
        }

        std::string tcdPath = argv[2];
        std::string outputDir = argv[3];

        if (ExtractFiles(tcdPath, outputDir)) {
            std::cout << "Extraction completed successfully." << std::endl;
            return 0;
        }
        else {
            std::cerr << "Extraction failed." << std::endl;
            return 1;
        }
    }
    else if (operation == "update") {  // 封包操作
        if (argc != 5) {
            std::cerr << "Usage for update: " << argv[0] << " -u <original_tcd> <update_directory> <output_tcd>" << std::endl;
            return 1;
        }

        std::string originalTcd = argv[2];
        std::string updateDir = argv[3];
        std::string outputTcd = argv[4];

        TcdUpdater updater(originalTcd, updateDir, outputTcd);
        if (updater.Update()) {
            std::cout << "Update completed successfully." << std::endl;
            return 0;
        }
        else {
            std::cerr << "Update failed." << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Invalid operation. Use -e for extract or -u for update." << std::endl;
        return 1;
    }
}
