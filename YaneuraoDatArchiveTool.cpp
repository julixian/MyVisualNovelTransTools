#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <map>
#include <iomanip>

namespace fs = std::filesystem;

struct FileEntry {
    std::string filename;
    uint32_t offset;
    uint32_t unpacked_size;
    uint32_t packed_size;
};

class DatExtractor {
public:
    DatExtractor(const std::string& datPath) : m_datPath(datPath) {}

    bool Extract(const std::string& outputDir) {
        std::ifstream file(m_datPath, std::ios::binary);
        if (!file) {
            std::cerr << "无法打开文件: " << m_datPath << std::endl;
            return false;
        }

        // 检查文件头
        char signature[8];
        file.read(signature, 8);

        bool isYanepkDx = (memcmp(signature, "yanepkDx", 8) == 0);
        bool isYanepkEx = (memcmp(signature, "yanepkEx", 8) == 0);

        if (!isYanepkDx && !isYanepkEx) {
            std::cerr << "不支持的文件格式" << std::endl;
            return false;
        }

        // 读取文件数量
        uint32_t count;
        file.seekg(8);
        file.read(reinterpret_cast<char*>(&count), 4);

        if (count > 10000) { // 安全检查
            std::cerr << "文件数量异常" << std::endl;
            return false;
        }

        // 读取文件条目
        std::vector<FileEntry> entries;
        entries.reserve(count);

        uint32_t nameLength = isYanepkDx ? 0x100 : 0x20;
        uint32_t indexOffset = 0xC;

        for (uint32_t i = 0; i < count; ++i) {
            FileEntry entry;

            // 读取文件名
            std::vector<char> nameBuffer(nameLength);
            file.seekg(indexOffset);
            file.read(nameBuffer.data(), nameLength);
            entry.filename = std::string(nameBuffer.data());

            // 读取文件信息
            file.seekg(indexOffset + nameLength);
            file.read(reinterpret_cast<char*>(&entry.offset), 4);
            file.read(reinterpret_cast<char*>(&entry.unpacked_size), 4);
            file.read(reinterpret_cast<char*>(&entry.packed_size), 4);

            entries.push_back(entry);
            indexOffset += nameLength + 0xC;
        }

        // 创建输出目录
        fs::create_directories(outputDir);

        // 提取文件
        for (const auto& entry : entries) {
            if (entry.filename.empty()) continue;

            // 创建输出路径
            std::string outPath = outputDir + "\\" + entry.filename;
            fs::path dirPath = fs::path(outPath).parent_path();
            fs::create_directories(dirPath);

            // 读取并写入文件数据
            std::vector<char> buffer(entry.packed_size);
            file.seekg(entry.offset);
            file.read(buffer.data(), entry.packed_size);

            std::ofstream outFile(outPath, std::ios::binary);
            if (!outFile) {
                std::cerr << "无法创建文件: " << outPath << std::endl;
                continue;
            }
            outFile.write(buffer.data(), entry.packed_size);

            std::cout << "已提取: " << entry.filename << std::endl;
        }

        return true;
    }

private:
    std::string m_datPath;
};

class DatUpdater {
public:
    DatUpdater(const std::string& datPath) : m_datPath(datPath) {}

    bool Update(const std::string& updateDir, const std::string& outputPath) {
        std::ifstream file(m_datPath, std::ios::binary);
        if (!file) {
            std::cerr << "无法打开文件: " << m_datPath << std::endl;
            return false;
        }

        // 检查文件头
        char signature[8];
        file.read(signature, 8);

        bool isYanepkDx = (memcmp(signature, "yanepkDx", 8) == 0);
        bool isYanepkEx = (memcmp(signature, "yanepkEx", 8) == 0);

        if (!isYanepkDx && !isYanepkEx) {
            std::cerr << "不支持的文件格式" << std::endl;
            return false;
        }

        // 读取文件数量
        uint32_t count;
        file.seekg(8);
        file.read(reinterpret_cast<char*>(&count), 4);

        if (count > 10000) { // 安全检查
            std::cerr << "文件数量异常" << std::endl;
            return false;
        }

        // 读取文件条目
        std::vector<FileEntry> entries;
        entries.reserve(count);

        uint32_t nameLength = isYanepkDx ? 0x100 : 0x20;
        uint32_t indexOffset = 0xC;
        uint32_t dataOffset = indexOffset + count * (nameLength + 0xC);

        std::cout << "封包中的文件列表：" << std::endl;
        std::cout << std::setw(5) << "序号" << std::setw(50) << "文件名" << std::setw(15) << "大小" << std::endl;
        std::cout << std::string(70, '-') << std::endl;

        for (uint32_t i = 0; i < count; ++i) {
            FileEntry entry;

            // 读取文件名
            std::vector<char> nameBuffer(nameLength);
            file.seekg(indexOffset);
            file.read(nameBuffer.data(), nameLength);
            entry.filename = std::string(nameBuffer.data());

            // 读取文件信息
            file.seekg(indexOffset + nameLength);
            file.read(reinterpret_cast<char*>(&entry.offset), 4);
            file.read(reinterpret_cast<char*>(&entry.unpacked_size), 4);
            file.read(reinterpret_cast<char*>(&entry.packed_size), 4);

            entries.push_back(entry);
            indexOffset += nameLength + 0xC;

            // 打印文件信息
            std::string cleanFilename = entry.filename;
            cleanFilename.erase(std::find(cleanFilename.begin(), cleanFilename.end(), '\0'), cleanFilename.end());
            std::cout << std::setw(5) << i + 1
                << std::setw(50) << cleanFilename
                << std::setw(15) << entry.packed_size << std::endl;
        }

        // 更新文件
        std::map<std::string, fs::path> updateFiles;
        std::cout << "\n更新目录中的文件：" << std::endl;
        for (const auto& entry : fs::recursive_directory_iterator(updateDir)) {
            if (entry.is_regular_file()) {
                std::string relativePath = fs::relative(entry.path(), updateDir).string();
                std::replace(relativePath.begin(), relativePath.end(), '/', '\\'); // 统一使用反斜杠
                updateFiles[relativePath] = entry.path();
                std::cout << "找到更新文件: " << relativePath << std::endl;
            }
        }

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "无法创建输出文件: " << outputPath << std::endl;
            return false;
        }

        // 写入文件头和文件数量
        outFile.write(signature, 8);
        outFile.write(reinterpret_cast<char*>(&count), 4);

        // 写入文件条目信息（先占位）
        uint32_t currentDataOffset = dataOffset;
        for (auto& entry : entries) {
            std::vector<char> nameBuffer(nameLength, 0);
            std::copy(entry.filename.begin(), entry.filename.end(), nameBuffer.begin());
            outFile.write(nameBuffer.data(), nameLength);
            outFile.write(reinterpret_cast<char*>(&currentDataOffset), 4);
            outFile.write(reinterpret_cast<char*>(&entry.unpacked_size), 4);
            outFile.write(reinterpret_cast<char*>(&entry.packed_size), 4);

            currentDataOffset += entry.packed_size;
        }

        // 写入文件数据并更新条目信息
        currentDataOffset = dataOffset;
        std::cout << "\n更新过程：" << std::endl;
        for (auto& entry : entries) {
            std::string cleanFilename = entry.filename;
            cleanFilename.erase(std::find(cleanFilename.begin(), cleanFilename.end(), '\0'), cleanFilename.end());
            std::replace(cleanFilename.begin(), cleanFilename.end(), '/', '\\'); // 统一使用反斜杠

            auto it = updateFiles.find(cleanFilename);
            if (it != updateFiles.end()) {
                // 使用更新的文件
                std::ifstream newFile(it->second, std::ios::binary);
                if (!newFile) {
                    std::cerr << "无法打开更新文件: " << it->second << std::endl;
                    continue;
                }
                newFile.seekg(0, std::ios::end);
                uint32_t newSize = newFile.tellg();
                newFile.seekg(0);

                std::vector<char> buffer(newSize);
                newFile.read(buffer.data(), newSize);
                outFile.write(buffer.data(), newSize);

                // 更新条目信息
                entry.offset = currentDataOffset;
                entry.packed_size = newSize;
                entry.unpacked_size = newSize; // 假设未压缩

                currentDataOffset += newSize;

                std::cout << "已更新: " << cleanFilename << " (新大小: " << newSize << " 字节)" << std::endl;
            }
            else {
                // 使用原始文件
                file.seekg(entry.offset);
                std::vector<char> buffer(entry.packed_size);
                file.read(buffer.data(), entry.packed_size);
                outFile.write(buffer.data(), entry.packed_size);

                entry.offset = currentDataOffset;
                currentDataOffset += entry.packed_size;

                std::cout << "未更新: " << cleanFilename << " (保持原大小: " << entry.packed_size << " 字节)" << std::endl;
            }
        }

        // 更新文件条目信息
        outFile.seekp(0xC);
        for (const auto& entry : entries) {
            std::vector<char> nameBuffer(nameLength, 0);
            std::copy(entry.filename.begin(), entry.filename.end(), nameBuffer.begin());
            outFile.write(nameBuffer.data(), nameLength);
            outFile.write(reinterpret_cast<const char*>(&entry.offset), 4);
            outFile.write(reinterpret_cast<const char*>(&entry.unpacked_size), 4);
            outFile.write(reinterpret_cast<const char*>(&entry.packed_size), 4);
        }

        std::cout << "\n更新完成，新文件已保存为: " << outputPath << std::endl;
        return true;
    }

private:
    std::string m_datPath;
};

void printUsage(const char* programName) {
    std::cout << "Made by julixian 2025.01.11" << std::endl;
    std::cout << "Usage：" << std::endl;
    std::cout << programName << " extract <input.dat> <output_dir>" << std::endl;
    std::cout << programName << " update <input.dat> <update_dir> <output.dat>" << std::endl;
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
        DatExtractor extractor(argv[2]);
        if (!extractor.Extract(argv[3])) {
            std::cerr << "extract fail" << std::endl;
            return 1;
        }
    }
    else if (command == "update") {
        if (argc != 5) {
            printUsage(argv[0]);
            return 1;
        }
        DatUpdater updater(argv[2]);
        if (!updater.Update(argv[3], argv[4])) {
            std::cerr << "update file" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
