#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iomanip>

namespace fs = std::filesystem;

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

class LzssDecompressor {
public:
    LzssDecompressor(int frameSize = 0x1000, uint8_t frameFill = 0, int frameInitPos = 0xFEE)
        : m_frameSize(frameSize), m_frameFill(frameFill), m_frameInitPos(frameInitPos) {
    }

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        std::vector<uint8_t> frame(m_frameSize, m_frameFill);
        int framePos = m_frameInitPos;
        int frameMask = m_frameSize - 1;

        size_t inputPos = 0;
        while (inputPos < input.size()) {
            uint8_t ctrl = input[inputPos++];
            for (int bit = 1; bit != 0x100 && inputPos < input.size(); bit <<= 1) {
                if (ctrl & bit) {
                    uint8_t b = input[inputPos++];
                    frame[framePos++ & frameMask] = b;
                    output.push_back(b);
                }
                else {
                    if (inputPos + 1 >= input.size()) break;
                    uint8_t lo = input[inputPos++];
                    uint8_t hi = input[inputPos++];
                    int offset = ((hi & 0xf0) << 4) | lo;
                    int count = 3 + (hi & 0xF);

                    for (int i = 0; i < count; ++i) {
                        uint8_t v = frame[offset++ & frameMask];
                        frame[framePos++ & frameMask] = v;
                        output.push_back(v);
                    }
                }
            }
        }

        return output;
    }

private:
    int m_frameSize;
    uint8_t m_frameFill;
    int m_frameInitPos;
};

struct BndEntry {
    uint32_t offset;
    uint32_t decomprlen;
    uint32_t size;
};

bool extractBndFiles(const std::string& bndFilePath, const std::string& outputDir) {
    // 打开BND文件
    std::ifstream bndFile(bndFilePath, std::ios::binary);
    if (!bndFile) {
        std::cerr << "无法打开BND文件: " << bndFilePath << std::endl;
        return false;
    }

    // 创建输出目录（如果不存在）
    if (!fs::exists(outputDir)) {
        if (!fs::create_directories(outputDir)) {
            std::cerr << "无法创建输出目录: " << outputDir << std::endl;
            return false;
        }
    }

    // 读取索引数目
    uint32_t entryCount;
    bndFile.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

    if (!bndFile) {
        std::cerr << "读取索引数目失败" << std::endl;
        return false;
    }

    std::cout << "BND文件包含 " << entryCount << " 个文件" << std::endl;

    // 读取所有索引
    std::vector<BndEntry> entries(entryCount);
    for (uint32_t i = 0; i < entryCount; ++i) {
        bndFile.read(reinterpret_cast<char*>(&entries[i]), sizeof(BndEntry));
        if (!bndFile) {
            std::cerr << "读取索引 #" << i << " 失败" << std::endl;
            return false;
        }
    }

    // 提取所有文件
    for (uint32_t i = 0; i < entryCount; ++i) {
        const BndEntry& entry = entries[i];

        // 生成输出文件名 (00000, 00001, ...)
        std::ostringstream fileNameStream;
        fileNameStream << std::setw(5) << std::setfill('0') << i;
        std::string outputFilePath = outputDir + "\\" + fileNameStream.str();

        // 创建输出文件
        std::ofstream outputFile(outputFilePath, std::ios::binary);
        if (!outputFile) {
            std::cerr << "无法创建输出文件: " << outputFilePath << std::endl;
            return false;
        }

        // 定位到文件数据
        bndFile.seekg(entry.offset, std::ios::beg);
        if (!bndFile) {
            std::cerr << "定位到文件 #" << i << " 数据失败" << std::endl;
            return false;
        }

        // 读取并写入文件数据
        std::vector<uint8_t> buffer(entry.size);
        bndFile.read((char*)buffer.data(), entry.size);

        LzssDecompressor lzssdecompressor;

        std::vector<uint8_t> decompressedData = lzssdecompressor.decompress(buffer);
        if (decompressedData.size() != entry.decomprlen) {
            std::cout << "Warning: unexpected data: " << i <<
                " expect:" << entry.decomprlen << "bytes" <<
                " Get:" << decompressedData.size() << "bytes" << std::endl;
        }


        outputFile.write((char*)decompressedData.data(), decompressedData.size());
        if (!outputFile) {
            std::cerr << "写入文件 #" << i << " 数据失败" << std::endl;
            return false;
        }

        std::cout << "已提取文件 " << fileNameStream.str()
            << " (偏移: " << entry.offset
            << ", 大小: " << entry.size << " 字节)" << std::endl;
    }

    std::cout << "所有文件提取完成!" << std::endl;
    return true;
}

// 新增封包功能
bool packBndFiles(const std::string& inputDir, const std::string& bndFilePath) {
    // 检查输入目录
    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cerr << "输入目录不存在或不是有效目录: " << inputDir << std::endl;
        return false;
    }

    // 收集目录中的所有文件
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    // 按文件名排序 (00000, 00001, ...)
    std::sort(files.begin(), files.end());

    // 创建BND文件
    std::ofstream bndFile(bndFilePath, std::ios::binary);
    if (!bndFile) {
        std::cerr << "无法创建BND文件: " << bndFilePath << std::endl;
        return false;
    }

    // 写入文件数目
    uint32_t fileCount = static_cast<uint32_t>(files.size());
    bndFile.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

    // 计算头部大小 (4字节文件数 + 每个文件12字节的索引)
    uint32_t headerSize = 4 + fileCount * sizeof(BndEntry);

    // 构建索引表和文件数据
    std::vector<BndEntry> entries(fileCount);
    std::vector<std::vector<uint8_t>> compressedData(fileCount);

    uint32_t currentOffset = headerSize;

    // 第一遍：读取文件，压缩数据，计算偏移
    for (uint32_t i = 0; i < fileCount; ++i) {
        // 读取原始文件
        std::ifstream inputFile(files[i], std::ios::binary);
        if (!inputFile) {
            std::cerr << "无法打开输入文件: " << files[i] << std::endl;
            return false;
        }

        // 获取文件大小
        inputFile.seekg(0, std::ios::end);
        size_t fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        // 读取文件内容
        std::vector<uint8_t> fileData(fileSize);
        inputFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);

        // 压缩文件数据
        compressedData[i] = compress(fileData);

        // 设置索引信息
        entries[i].offset = currentOffset;
        entries[i].decomprlen = static_cast<uint32_t>(fileData.size());
        entries[i].size = static_cast<uint32_t>(compressedData[i].size());

        // 更新下一个文件的偏移
        currentOffset += entries[i].size;

        std::cout << "准备打包文件 " << files[i].filename().string()
            << " (原始大小: " << fileData.size()
            << " 字节, 压缩后: " << compressedData[i].size() << " 字节)" << std::endl;
    }

    // 写入索引表
    for (uint32_t i = 0; i < fileCount; ++i) {
        bndFile.write(reinterpret_cast<const char*>(&entries[i]), sizeof(BndEntry));
    }

    // 写入文件数据
    for (uint32_t i = 0; i < fileCount; ++i) {
        bndFile.write(reinterpret_cast<const char*>(compressedData[i].data()), compressedData[i].size());

        std::cout << "已打包文件 " << files[i].filename().string()
            << " (偏移: " << entries[i].offset
            << ", 压缩大小: " << entries[i].size << " 字节)" << std::endl;
    }

    std::cout << "所有文件打包完成! 共 " << fileCount << " 个文件" << std::endl;
    return true;
}

void printUsage(const char* programName) {
    std::cout << "Usage:" << std::endl;
    std::cout << "Made by julixian 2025.04.11" << std::endl;
    std::cout << "  extract: " << programName << " -e <BND_FILE> <OUTPUT_DIR>" << std::endl;
    std::cout << "  pack: " << programName << " -p <INPUT_DIR> <OUTPUT_BND_FILE>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    std::string path1 = argv[2];
    std::string path2 = argv[3];

    if (mode == "-e") {
        // 解包模式
        if (!extractBndFiles(path1, path2)) {
            std::cerr << "BND文件提取失败" << std::endl;
            return 1;
        }
    }
    else if (mode == "-p") {
        // 封包模式
        if (!packBndFiles(path1, path2)) {
            std::cerr << "BND文件打包失败" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "无效的模式: " << mode << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
