#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;

std::string wide2Ascii(const std::wstring& wide, UINT CodePage = CP_UTF8);
std::wstring ascii2Wide(const std::string& ascii, UINT CodePage = CP_ACP);
std::string ascii2Ascii(const std::string& ascii, UINT src = CP_ACP, UINT dst = CP_UTF8);

std::string wide2Ascii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst) {
    return wide2Ascii(ascii2Wide(ascii, src), dst);
}

template<typename EntryType>
void fixProFilePos(std::vector<EntryType>& entries) {
    if constexpr (std::is_same_v<decltype(EntryType::name), std::wstring>) {
        auto it = std::ranges::find_if(entries, [](const auto& entry)
            {
                return entry.name == L"pro.txt";
            });
        if (it != entries.end() && it != entries.begin()) {
            std::swap(entries[0], *it);
        }
    }
    else if constexpr (std::is_same_v<decltype(EntryType::name), std::string>) {
        auto it = std::ranges::find_if(entries, [](const auto& entry)
            {
                return entry.name == "pro.txt";
            });
        if (it != entries.end() && it != entries.begin()) {
            std::swap(entries[0], *it);
        }
    }
    else {
        static_assert(false, "Unsupported EntryType");
    }
}

// 大端字节序转换
uint32_t changeEndian(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x000000FF) << 24);
}

// 密钥生成
uint8_t keyFromOffset(uint32_t offset) {
    const uint32_t BASE = 0x5CC8E9D7;
    uint32_t v1 = offset - BASE + (0xA3371629u >> ((offset & 0xF) + 1)) - (BASE << (31 - (offset & 0xF)));

    uint32_t v3 = v1 << (31 - ((offset >> 4) & 0xF));
    uint32_t v4 = v1 >> (((offset >> 4) & 0xF) + 1);
    uint32_t v5 = offset - BASE
        + ((offset - BASE + v3 + v4) << (31 - ((offset >> 8) & 0xF)))
        + ((offset - BASE + v3 + v4) >> (((offset >> 8) & 0xF) + 1));
    uint32_t v6 = offset - BASE
        + (v5 << (31 - ((offset >> 12) & 0xF))) + (v5 >> (((offset >> 12) & 0xF) + 1));
    uint32_t v7 = offset - BASE
        + (v6 << (31 - ((offset >> 16) & 0xF))) + (v6 >> (((offset >> 16) & 0xF) + 1));
    int v8 = (offset >> 20) & 0xF;
    uint32_t v9 = offset - BASE
        + ((offset - BASE + (v7 << (31 - v8)) + (v7 >> (v8 + 1))) << (31 - ((offset >> 24) & 0xF)))
        + ((offset - BASE + (v7 << (31 - v8)) + (v7 >> (v8 + 1))) >> (((offset >> 24) & 0xF) + 1));
    uint32_t key = (offset - BASE + (v9 << (31 - (offset >> 28))) + (v9 >> ((offset >> 28) + 1))) >> (offset & 0xF);
    return static_cast<uint8_t>(key);
}

namespace AOIMY01U 
{
    // 文件条目结构
    struct Entry {
        std::wstring name; // 32字节的宽字符名称
        uint32_t offset;
        uint32_t size;
    };

    // 解包函数
    void extractBox(const fs::path& boxPath, const fs::path& outputDir) {
        std::ifstream ifs(boxPath, std::ios::binary);
        if (!ifs) {
            throw std::runtime_error(std::format("无法打开文件: {}", wide2Ascii(boxPath.wstring())));
        }

        ifs.seekg(16);

        uint32_t fileCount;
        ifs.read(reinterpret_cast<char*>(&fileCount), 4);
        fileCount = changeEndian(fileCount);

        // 跳过保留的4字节
        ifs.seekg(4, std::ios::cur);

        std::vector<Entry> entries;
        for (uint32_t i = 0; i < fileCount; ++i) {
            Entry entry;

            std::vector<char> nameBuffer(32);
            ifs.read(nameBuffer.data(), 32);
            entry.name = std::wstring(reinterpret_cast<wchar_t*>(nameBuffer.data()));

            // 读取偏移和大小
            uint32_t beOffset, beSize;
            ifs.read(reinterpret_cast<char*>(&beOffset), 4);
            ifs.read(reinterpret_cast<char*>(&beSize), 4);
            entry.offset = changeEndian(beOffset);
            entry.size = changeEndian(beSize);

            entries.push_back(entry);
        }

        // 提取文件
        std::ofstream ofs;
        for (const auto& entry : entries) {
            std::println("Extracting file: {}", wide2Ascii(entry.name));

            // 读取文件数据
            std::vector<uint8_t> data(entry.size);
            ifs.seekg(entry.offset);
            ifs.read(reinterpret_cast<char*>(data.data()), entry.size);

            // 解密数据
            uint32_t offset = entry.offset;
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] ^= keyFromOffset(offset++);
            }

            // 写入文件
            const fs::path outFilePath = outputDir / entry.name;
            fs::create_directories(outFilePath.parent_path());
            ofs.open(outFilePath, std::ios::binary);
            if (!ofs.is_open()) {
                throw std::runtime_error(std::format("无法创建文件: {}", wide2Ascii(outFilePath.wstring())));
            }
            ofs.write(reinterpret_cast<char*>(data.data()), data.size());
            ofs.close();
        }

        std::println("Extracting completed.");
    }

    // 封包函数
    void createBox(const fs::path& inputDir, const fs::path& boxPath) {
        // 收集文件信息
        std::vector<Entry> entries;
        uint32_t currentOffset = 0;

        // 计算索引区大小
        currentOffset = 24;

        // 收集目录中的所有文件
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (!entry.is_regular_file()) continue;

            Entry fileEntry;
            fileEntry.name = fs::relative(entry.path(), inputDir).wstring();
            fileEntry.size = static_cast<uint32_t>(entry.file_size());
            entries.push_back(fileEntry);
        }

        if (entries.empty()) {
            throw std::runtime_error("没有找到文件.");
        }
        fixProFilePos(entries);

        // 计算每个文件的偏移
        currentOffset += static_cast<uint32_t>(entries.size() * 0x28);
        for (auto& entry : entries) {
            entry.offset = currentOffset;
            currentOffset += entry.size;
        }

        // 创建box文件
        std::ofstream ofs(boxPath, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error(std::format("无法创建文件: {}", wide2Ascii(boxPath.wstring())));
        }

        // 写入文件头
        const wchar_t header[] = L"AOIMY01\0";
        ofs.write(reinterpret_cast<const char*>(header), 16);

        // 写入文件数量（大端字节序）
        uint32_t fileCount = changeEndian(static_cast<uint32_t>(entries.size()));
        ofs.write(reinterpret_cast<char*>(&fileCount), 4);

        // 写入保留的4字节
        uint32_t reserved = 0;
        ofs.write(reinterpret_cast<char*>(&reserved), 4);

        // 写入文件条目
        for (const auto& entry : entries) {
            // 写入文件名（32字节，不足部分填充0）
            if (entry.name.length() * sizeof(wchar_t) >= 32) {
                throw std::runtime_error(std::format("文件名过长: {}", wide2Ascii(entry.name)));
            }
            std::vector<char> nameBuffer(32, 0);
            memcpy(nameBuffer.data(), entry.name.c_str(), entry.name.length() * sizeof(wchar_t));
            ofs.write(nameBuffer.data(), 32);

            // 写入偏移和大小（大端字节序）
            uint32_t beOffset = changeEndian(entry.offset);
            uint32_t beSize = changeEndian(entry.size);
            ofs.write(reinterpret_cast<char*>(&beOffset), 4);
            ofs.write(reinterpret_cast<char*>(&beSize), 4);
        }

        // 写入文件数据
        std::ifstream ifs;
        for (const auto& entry : entries) {
            std::println("Packing file: {}", wide2Ascii(entry.name));

            ifs.open(inputDir / entry.name, std::ios::binary);
            if (!ifs.is_open()) {
                throw std::runtime_error(std::format("无法打开文件: {}", wide2Ascii(entry.name)));
            }

            std::vector<uint8_t> data(entry.size);
            ifs.read(reinterpret_cast<char*>(data.data()), entry.size);
            ifs.close();

            uint32_t offset = entry.offset;
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] ^= keyFromOffset(offset++);
            }

            ofs.write(reinterpret_cast<char*>(data.data()), data.size());
        }

        ofs.close();
        std::println("Packing completed.");
    }
}

namespace AOIMY01A
{
    // 文件条目结构
    struct Entry {
        std::string name; // 12字节的ASCII名称
        uint32_t unknown; // 0x1
        uint32_t offset;
        uint32_t size;
    };

    // 解包函数
    void extractBox(const fs::path& boxPath, const fs::path& outputDir) {
        std::ifstream ifs(boxPath, std::ios::binary);
        if (!ifs) {
            throw std::runtime_error(std::format("无法打开文件: {}", wide2Ascii(boxPath.wstring())));
        }

        ifs.seekg(8);

        uint32_t fileCount;
        ifs.read(reinterpret_cast<char*>(&fileCount), 4);
        fileCount = changeEndian(fileCount);

        ifs.seekg(4, std::ios::cur);

        std::vector<Entry> entries;
        for (uint32_t i = 0; i < fileCount; ++i) {
            Entry entry;

            std::vector<char> nameBuffer(12);
            ifs.read(nameBuffer.data(), 12);
            entry.name = std::string(reinterpret_cast<char*>(nameBuffer.data()));
            ifs.seekg(4, std::ios::cur);

            uint32_t beOffset, beSize;
            ifs.read(reinterpret_cast<char*>(&beOffset), 4);
            ifs.read(reinterpret_cast<char*>(&beSize), 4);
            entry.offset = changeEndian(beOffset);
            entry.size = changeEndian(beSize);

            entries.push_back(entry);
        }

        std::ofstream ofs;
        for (const auto& entry : entries) {
            std::println("Extracting file: {}", ascii2Ascii(entry.name, 932));

            // 读取文件数据
            std::vector<uint8_t> data(entry.size);
            ifs.seekg(entry.offset);
            ifs.read(reinterpret_cast<char*>(data.data()), entry.size);

            uint32_t offset = entry.offset;
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] ^= keyFromOffset(offset++);
            }

            // 写入文件
            const fs::path outPath = outputDir / ascii2Wide(entry.name, 932);
            fs::create_directories(outPath.parent_path());
            ofs.open(outPath, std::ios::binary);
            if (!ofs.is_open()) {
                throw std::runtime_error(std::format("无法创建文件: {}", wide2Ascii(outPath.wstring())));
            }
            ofs.write(reinterpret_cast<char*>(data.data()), data.size());
            ofs.close();
        }

        std::println("Extracting completed.");
    }

    // 封包函数
    void createBox(const fs::path& inputDir, const fs::path& boxPath) {
        // 收集文件信息
        std::vector<Entry> entries;
        uint32_t currentOffset = 0;

        // 计算索引区大小
        currentOffset = 16;

        // 收集目录中的所有文件
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (!entry.is_regular_file()) continue;

            Entry fileEntry;
            fileEntry.name = wide2Ascii(fs::relative(entry.path(), inputDir), 932);
            fileEntry.size = static_cast<uint32_t>(entry.file_size());
            entries.push_back(fileEntry);
        }

        if (entries.empty()) {
            throw std::runtime_error("没有找到文件.");
        }
        fixProFilePos(entries);

        // 计算每个文件的偏移
        currentOffset += static_cast<uint32_t>(entries.size() * 0x18);
        for (auto& entry : entries) {
            entry.offset = currentOffset;
            currentOffset += entry.size;
        }

        // 创建box文件
        std::ofstream ofs(boxPath, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error(std::format("无法创建文件: {}", wide2Ascii(boxPath.wstring())));
        }

        // 写入文件头
        const char header[] = "AOIMY01\0";
        ofs.write(reinterpret_cast<const char*>(header), 8);

        // 写入文件数量
        uint32_t count = static_cast<uint32_t>(entries.size());
        uint32_t beCount = changeEndian(count);
        ofs.write(reinterpret_cast<char*>(&beCount), 4);

        // 写入保留的4字节
        uint32_t reserved = 0;
        ofs.write(reinterpret_cast<char*>(&reserved), 4);

        // 写入文件条目
        for (const auto& entry : entries) {
            // 写入文件名（16字节，不足部分填充0）
            if (entry.name.length() * sizeof(char) >= 12) {
                throw std::runtime_error(std::format("文件名过长: {}", ascii2Ascii(entry.name, 932)));
            }
            std::vector<char> nameBuffer(12, 0);
            memcpy(nameBuffer.data(), entry.name.c_str(), entry.name.length() * sizeof(char));
            ofs.write(nameBuffer.data(), 12);

            uint32_t unknown = 0x1;
            ofs.write(reinterpret_cast<const char*>(&unknown), 4);

            // 写入偏移和大小
            uint32_t beOffset = changeEndian(entry.offset);
            uint32_t beSize = changeEndian(entry.size);
            ofs.write(reinterpret_cast<char*>(&beOffset), 4);
            ofs.write(reinterpret_cast<char*>(&beSize), 4);
        }

        // 写入文件数据
        std::ifstream ifs;
        for (const auto& entry : entries) {
            std::println("Packing file: {}", ascii2Ascii(entry.name, 932));

            ifs.open(inputDir / ascii2Wide(entry.name, 932), std::ios::binary);
            if (!ifs.is_open()) {
                throw std::runtime_error(std::format("无法打开文件: {}", ascii2Ascii(entry.name, 932)));
            }
            std::vector<uint8_t> data(entry.size);
            ifs.read(reinterpret_cast<char*>(data.data()), entry.size);
            ifs.close();

            uint32_t offset = entry.offset;
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] ^= keyFromOffset(offset++);
            }

            ofs.write(reinterpret_cast<char*>(data.data()), data.size());
        }

        ofs.close();
        std::println("Packing completed.");
    }
}

namespace AOIBX9A
{
    // 文件条目结构
    struct Entry {
        std::string name; // 16字节的ASCII名称
        uint32_t offset;
        uint32_t size;
    };

    // 解包函数
    void extractBox(const fs::path& boxPath, const fs::path& outputDir) {
        std::ifstream ifs(boxPath, std::ios::binary);
        if (!ifs) {
            throw std::runtime_error(std::format("无法打开文件: {}", wide2Ascii(boxPath.wstring())));
        }

        ifs.seekg(8);

        uint32_t fileCount;
        ifs.read(reinterpret_cast<char*>(&fileCount), 4);

        ifs.seekg(4, std::ios::cur);

        std::vector<Entry> entries;
        for (uint32_t i = 0; i < fileCount; ++i) {
            Entry entry;

            std::vector<char> nameBuffer(16);
            ifs.read(nameBuffer.data(), 16);
            entry.name = std::string(reinterpret_cast<char*>(nameBuffer.data()));

            ifs.read(reinterpret_cast<char*>(&entry.offset), 4);
            ifs.read(reinterpret_cast<char*>(&entry.size), 4);

            entries.push_back(entry);
        }

        std::ofstream ofs;
        for (const auto& entry : entries) {
            std::println("Extracting file: {}", ascii2Ascii(entry.name, 932));

            // 读取文件数据
            std::vector<uint8_t> data(entry.size);
            ifs.seekg(entry.offset);
            ifs.read(reinterpret_cast<char*>(data.data()), entry.size);

            for (size_t i = 0; i < data.size(); ++i) {
                data[i] ^= 0x5f;
            }

            // 写入文件
            const fs::path outPath = outputDir / ascii2Wide(entry.name, 932);
            fs::create_directories(outPath.parent_path());
            ofs.open(outPath, std::ios::binary);
            if (!ofs.is_open()) {
                throw std::runtime_error(std::format("无法创建文件: {}", wide2Ascii(outPath.wstring())));
            }
            ofs.write(reinterpret_cast<char*>(data.data()), data.size());
            ofs.close();
        }

        std::println("Extracting completed.");
    }

    // 封包函数
    void createBox(const fs::path& inputDir, const fs::path& boxPath) {
        // 收集文件信息
        std::vector<Entry> entries;
        uint32_t currentOffset = 0;

        // 计算索引区大小
        currentOffset = 16;

        // 收集目录中的所有文件
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (!entry.is_regular_file()) continue;

            Entry fileEntry;
            fileEntry.name = wide2Ascii(fs::relative(entry.path(), inputDir), 932);
            fileEntry.size = static_cast<uint32_t>(entry.file_size());
            entries.push_back(fileEntry);
        }

        if (entries.empty()) {
            throw std::runtime_error("没有找到文件.");
        }
        fixProFilePos(entries);

        // 计算每个文件的偏移
        currentOffset += static_cast<uint32_t>(entries.size() * 0x18);
        for (auto& entry : entries) {
            entry.offset = currentOffset;
            currentOffset += entry.size;
        }

        // 创建box文件
        std::ofstream ofs(boxPath, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error(std::format("无法创建文件: {}", wide2Ascii(boxPath.wstring())));
        }

        // 写入文件头
        const char header[] = "AOIBX9\0\0";
        ofs.write(reinterpret_cast<const char*>(header), 8);

        // 写入文件数量
        uint32_t count = static_cast<uint32_t>(entries.size());
        ofs.write(reinterpret_cast<char*>(&count), 4);

        // 写入保留的4字节
        uint32_t reserved = 0;
        ofs.write(reinterpret_cast<char*>(&reserved), 4);

        // 写入文件条目
        for (const auto& entry : entries) {
            // 写入文件名（16字节，不足部分填充0）
            if (entry.name.length() * sizeof(char) >= 0x10) {
                throw std::runtime_error(std::format("文件名过长: {}", ascii2Ascii(entry.name, 932)));
            }
            std::vector<char> nameBuffer(0x10, 0);
            memcpy(nameBuffer.data(), entry.name.c_str(), entry.name.length() * sizeof(char));
            ofs.write(nameBuffer.data(), 0x10);

            // 写入偏移和大小
            ofs.write(reinterpret_cast<const char*>(&entry.offset), 4);
            ofs.write(reinterpret_cast<const char*>(&entry.size), 4);
        }

        // 写入文件数据
        std::ifstream ifs;
        for (const auto& entry : entries) {
            std::println("Packing file: {}", ascii2Ascii(entry.name, 932));

            ifs.open(inputDir / ascii2Wide(entry.name, 932), std::ios::binary);
            if (!ifs.is_open()) {
                throw std::runtime_error(std::format("无法打开文件: {}", ascii2Ascii(entry.name, 932)));
            }
            std::vector<uint8_t> data(entry.size);
            ifs.read(reinterpret_cast<char*>(data.data()), entry.size);
            ifs.close();

            for (size_t i = 0; i < data.size(); ++i) {
                data[i] ^= 0x5f;
            }

            ofs.write(reinterpret_cast<char*>(data.data()), data.size());
        }

        ofs.close();
        std::println("Packing completed.");
    }
}

void chooseExtractMode(const fs::path& boxPath, const fs::path& outputDir) {
    std::ifstream ifs(boxPath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error(std::format("无法打开文件: {}", wide2Ascii(boxPath.wstring())));
    }

    std::vector<char> header(18, 0);
    ifs.read(header.data(), 16);
    ifs.close();

    if (std::string(header.data()) == "AOIMY01") {
        AOIMY01A::extractBox(boxPath, outputDir);
    }
    else if (std::string(header.data()) == "AOIBX9") {
        AOIBX9A::extractBox(boxPath, outputDir);
    }
    else if (std::wstring((wchar_t*)header.data()) == L"AOIMY01") {
        AOIMY01U::extractBox(boxPath, outputDir);
    }
    else {
        throw std::runtime_error(std::format("Unsupported format: {}", wide2Ascii(boxPath.wstring())));
    }
}

void choosePackMode(const std::wstring& mode, const fs::path& inputDir, const fs::path& outputBoxPath) {
    if (mode == L"1") {
        AOIMY01U::createBox(inputDir, outputBoxPath);
    }
    else if (mode == L"2") {
        AOIMY01A::createBox(inputDir, outputBoxPath);
    }
    else if (mode == L"3") {
        AOIBX9A::createBox(inputDir, outputBoxPath);
    }
    else {
        throw std::runtime_error(std::format("Unknown mode: {}", wide2Ascii(mode)));
    }
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.21\n"
        "Usage:\n"
        " For extract: {0} -e <box_file> <output_dir>\n"
        " For pack: {0} -p <mode> <input_dir> <output_box_file>\n"
        " mode: 1 - AOIMY01/Unicode, 2 - AOIMY01/ANSI, 3 - AOIBX9/ANSI\n",
        wide2Ascii(programPath.filename()));
}

int wmain(int argc, wchar_t* argv[]) {
    
    SetConsoleOutputCP(CP_UTF8);

    try {
        if (argc < 2) {
            printUsage(argv[0]);
            return 1;
        }

        std::wstring mode = argv[1];

        if (mode == L"-e") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            const fs::path boxPath = argv[2];
            const fs::path outputDir = argv[3];
            chooseExtractMode(boxPath, outputDir);
        }
        else if (mode == L"-p") {
            if (argc < 5) {
                printUsage(argv[0]);
                return 1;
            }
            const std::wstring mode = argv[2];
            const fs::path inputDir = argv[3];
            const fs::path outputBoxPath = argv[4];
            choosePackMode(mode, inputDir, outputBoxPath);
        }
        else {
            std::println("Error: Invalid mode: {}", wide2Ascii(mode));
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::println("Error: {}", e.what());
        return 1;
    }

    return 0;
}
