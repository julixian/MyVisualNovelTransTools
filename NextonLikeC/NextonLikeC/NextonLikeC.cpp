#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;

// 文件类型对应的扩展名
const std::string TYPE_EXT[] = { "LST", "SNX", "BMP", "PNG", "WAV", "OGG" };

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

// 条目结构
struct Entry {
    std::string fileName;   // 显示用的文件名（包含扩展名，用于查找替换文件）
    uint32_t offset;           // 在封包中的偏移
    uint32_t size;             // 文件大小
    uint8_t key;               // 解密密钥 (0表示不需要解密)
    std::string type;          // 文件类型
    int32_t typeIndex;         // 类型索引
    bool replaced;             // 是否被替换
    std::vector<char> data;    // 文件数据
};

// 封包格式
enum class PackFormat {
    Unknown,
    Moon,
    Nexton
};

// 封包信息
struct PackInfo {
    PackFormat format;
    uint8_t key;
    uint32_t entrySize;  // Moon: 0x2c, Nexton: 0x4c
    std::vector<Entry> entries;
};

// 读取并解密名称
std::string readName(std::ifstream& file, uint32_t offset, uint32_t size, uint8_t key) {
    file.seekg(offset);
    std::vector<char> buffer(size, 0);
    file.read(buffer.data(), size);

    std::string result;
    for (uint32_t i = 0; i < size; ++i) {
        if (buffer[i] == 0)
            break;

        uint8_t b = static_cast<uint8_t>(buffer[i]);
        if (b != key)
            b ^= key;

        // 只处理有效字符
        if (b != 0)
            result.push_back(static_cast<char>(b));
    }

    return result;
}

// 写入并加密名称
void writeName(std::ofstream& file, const std::string& name, uint32_t offset, uint32_t size, uint8_t key) {
    std::vector<char> buffer(size, 0);

    // 复制名称，确保不超过缓冲区大小
    for (size_t i = 0; i < name.size() && i < size - 1; ++i) {
        uint8_t b = static_cast<uint8_t>(name[i]);
        if (b != key)
            b ^= key;
        buffer[i] = b;
    }

    // 写入文件
    file.seekp(offset);
    file.write(buffer.data(), size);
}

// 尝试以Moon格式打开列表文件
std::vector<Entry> openMoon(std::ifstream& lst, uint64_t maxOffset) {
    std::vector<Entry> entries;

    // 读取文件头
    lst.seekg(0);
    uint32_t countEncrypted;
    lst.read(reinterpret_cast<char*>(&countEncrypted), 4);

    uint32_t count = countEncrypted ^ 0xcccccccc;

    // 验证条目数量
    if (count <= 0 || (4 + count * 0x2c) > maxOffset) {
        return {};
    }

    // 读取每个条目
    entries.reserve(count);
    uint32_t indexOffset = 4;

    for (uint32_t i = 0; i < count; ++i) {
        lst.seekg(indexOffset);

        uint32_t offsetEncrypted, sizeEncrypted;
        lst.read(reinterpret_cast<char*>(&offsetEncrypted), 4);
        lst.read(reinterpret_cast<char*>(&sizeEncrypted), 4);

        uint32_t offset = offsetEncrypted ^ 0xcccccccc;
        uint32_t size = sizeEncrypted ^ 0xcccccccc;

        // 读取文件名
        std::string name = readName(lst, indexOffset + 8, 0x24, 0xcc);
        name = ascii2Ascii(name, 932, CP_UTF8);

        // 验证
        if (name.empty() || offset + size > maxOffset) {
            return {};
        }

        Entry entry;
        entry.fileName = name;
        entry.offset = offset;
        entry.size = size;
        entry.key = 0;
        entry.typeIndex = -1;
        entry.replaced = false;

        entries.push_back(entry);
        indexOffset += 0x2c;
    }

    return entries;
}

// 尝试以Nexton格式打开列表文件
std::vector<Entry> openNexton(std::ifstream& lst, uint64_t maxOffset) {
    std::vector<Entry> entries;

    // 猜测XOR密钥
    lst.seekg(3);
    uint8_t keyByte;
    lst.read(reinterpret_cast<char*>(&keyByte), 1);

    if (keyByte == 0) {
        return {};
    }

    uint32_t key = keyByte;
    key |= key << 8;
    key |= key << 16;

    // 读取文件头
    lst.seekg(0);
    uint32_t countEncrypted;
    lst.read(reinterpret_cast<char*>(&countEncrypted), 4);

    uint32_t count = countEncrypted ^ key;

    // 验证条目数量
    if (count <= 0 || (4 + count * 0x4c) > maxOffset) {
        return {};
    }

    // 读取每个条目
    entries.reserve(count);
    uint32_t indexOffset = 4;

    for (uint32_t i = 0; i < count; ++i) {
        lst.seekg(indexOffset);

        uint32_t offsetEncrypted, sizeEncrypted;
        lst.read(reinterpret_cast<char*>(&offsetEncrypted), 4);
        lst.read(reinterpret_cast<char*>(&sizeEncrypted), 4);

        uint32_t offset = offsetEncrypted ^ key;
        uint32_t size = sizeEncrypted ^ key;

        // 读取文件名
        std::string name = readName(lst, indexOffset + 8, 0x40, keyByte);
        name = ascii2Ascii(name, 932, CP_UTF8);

        // 验证
        if (name.empty() || offset + size > maxOffset) {
            return {};
        }

        Entry entry;
        entry.fileName = name;
        entry.offset = offset;
        entry.size = size;
        entry.key = 0;
        entry.typeIndex = -1;
        entry.replaced = false;

        // 读取类型
        lst.seekg(indexOffset + 0x48);
        int32_t type;
        lst.read(reinterpret_cast<char*>(&type), 4);

        if (type >= 0 && type < 6) {
            entry.typeIndex = type;

            // 设置正确的扩展名
            size_t dotPos = entry.fileName.find_last_of('.');
            if (dotPos != std::string::npos) {
                entry.fileName = entry.fileName.substr(0, dotPos);
            }
            entry.fileName += "." + TYPE_EXT[type];

            // 设置文件类型
            if (type == 2 || type == 3) {
                entry.type = "image";
            }
            else if (type == 4 || type == 5) {
                entry.type = "audio";
            }
            else if (type == 1) {
                entry.type = "script";
                entry.key = keyByte + 1;
            }
        }

        entries.push_back(entry);
        indexOffset += 0x4c;
    }

    return entries;
}

// 读取文件内容
std::vector<char> readFileData(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return {};
    }
    std::vector<char> buffer(fs::file_size(filename));
    file.read(buffer.data(), buffer.size());
    return buffer;
}

// 读取原始封包中的文件数据
void readOriginalData(std::ifstream& arc, Entry& entry) {
    entry.data.resize(entry.size);
    arc.seekg(entry.offset);
    arc.read(entry.data.data(), entry.size);
    // 如果需要解密
    if (entry.key != 0) {
        for (size_t i = 0; i < entry.data.size(); ++i) {
            entry.data[i] ^= entry.key;
        }
    }
}

// 查找替换文件
bool findReplaceFile(const fs::path& replaceDir, Entry& entry) {
    // 尝试查找完全匹配的文件名
    const fs::path replacePath = replaceDir / ascii2Wide(entry.fileName, CP_UTF8);
    if (fs::exists(replacePath) && fs::is_regular_file(replacePath)) {
        std::vector<char> newData = readFileData(replacePath.string());
        if (!newData.empty()) {
            entry.data = std::move(newData);
            entry.size = static_cast<uint32_t>(entry.data.size());
            entry.replaced = true;
            return true;
        }
    }

    return false;
}

// 分析封包格式
PackInfo analyzePackage(const fs::path& arcFilePath, const fs::path& lstFilePath) {
    PackInfo info;
    info.format = PackFormat::Unknown;

    // 打开文件
    std::ifstream arc(arcFilePath, std::ios::binary);
    std::ifstream lst(lstFilePath, std::ios::binary);

    if (!arc || !lst) {
        std::println("Cannot open file: {}", wide2Ascii(arcFilePath));
        return info;
    }

    // 获取文件大小
    uint64_t maxOffset = fs::file_size(arcFilePath);
    uint64_t lstSize = fs::file_size(lstFilePath);

    // 先尝试Moon格式
    info.entries = openMoon(lst, maxOffset);
    if (!info.entries.empty()) {
        info.format = PackFormat::Moon;
        info.key = 0xcc;
        info.entrySize = 0x2c;
        return info;
    }

    // 再尝试Nexton格式
    info.entries = openNexton(lst, maxOffset);
    if (!info.entries.empty()) {
        info.format = PackFormat::Nexton;
        // 获取猜测的密钥
        lst.seekg(3);
        lst.read(reinterpret_cast<char*>(&info.key), 1);
        info.entrySize = 0x4c;
        return info;
    }

    return info;
}

// 提取文件
void extractFile(std::ifstream& arc, const Entry& entry, const fs::path& outputDir) {

    // 打开输出文件
    const fs::path outputPath = outputDir / ascii2Wide(entry.fileName, CP_UTF8);
    fs::create_directories(outputPath.parent_path());
    std::ofstream ofs(outputPath, std::ios::binary);
    if (!ofs) {
        std::println("Cannot create file: {}", wide2Ascii(outputPath));
        return;
    }

    // 读取数据
    std::vector<char> buffer(entry.size);
    arc.seekg(entry.offset);
    arc.read(buffer.data(), entry.size);

    // 如果需要解密
    if (entry.key != 0) {
        for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] ^= entry.key;
        }
    }

    // 写入数据
    ofs.write(buffer.data(), buffer.size());
    ofs.close();
    std::println("Extracted: {} ({} bytes)", wide2Ascii(outputPath), entry.size);
}

// 创建新的封包和LST文件
void createNewPackage(PackInfo& info, const fs::path& arcFilePath,
    const fs::path& lstFilePath,
    const fs::path& newArcFilePath,
    const fs::path& newArcLstPath) {
    // 打开原始LST文件进行读取
    std::ifstream oldLst(lstFilePath, std::ios::binary);
    if (!oldLst) {
        throw std::runtime_error(std::format("Cannot open file: {}", wide2Ascii(lstFilePath)));
    }

    // 创建新文件
    std::ofstream newArc(newArcFilePath, std::ios::binary);
    std::ofstream newLst(newArcLstPath, std::ios::binary);

    if (!newArc || !newLst) {
        throw std::runtime_error(std::format("Cannot create file: {} or {}", wide2Ascii(newArcFilePath), wide2Ascii(newArcLstPath)));
    }

    // 写入LST文件头
    uint32_t count = static_cast<uint32_t>(info.entries.size());
    uint32_t countEncrypted;

    if (info.format == PackFormat::Moon) {
        countEncrypted = count ^ 0xcccccccc;
    }
    else {
        uint32_t key = info.key;
        key |= key << 8;
        key |= key << 16;
        countEncrypted = count ^ key;
    }

    newLst.write(reinterpret_cast<char*>(&countEncrypted), 4);

    // 计算新的文件偏移
    uint32_t currentOffset = 0;
    for (auto& entry : info.entries) {
        entry.offset = currentOffset;
        currentOffset += entry.size;
    }

    // 复制原始LST文件内容并更新偏移和大小
    std::vector<char> lstBuffer(fs::file_size(lstFilePath));
    oldLst.seekg(0);
    oldLst.read(lstBuffer.data(), lstBuffer.size());

    // 写入LST文件条目
    uint32_t indexOffset = 4;
    for (const auto& entry : info.entries) {
        uint32_t offsetEncrypted, sizeEncrypted;

        if (info.format == PackFormat::Moon) {
            offsetEncrypted = entry.offset ^ 0xcccccccc;
            sizeEncrypted = entry.size ^ 0xcccccccc;

            // 写入偏移和大小
            newLst.seekp(indexOffset);
            newLst.write(reinterpret_cast<char*>(&offsetEncrypted), 4);
            newLst.write(reinterpret_cast<char*>(&sizeEncrypted), 4);

            // 复制原始文件名 - 直接从原始LST文件复制
            oldLst.seekg(indexOffset + 8);
            std::vector<char> nameBuffer(0x24);
            oldLst.read(nameBuffer.data(), 0x24);
            newLst.seekp(indexOffset + 8);
            newLst.write(nameBuffer.data(), 0x24);

            indexOffset += 0x2c;
        }
        else {
            uint32_t key = info.key;
            key |= key << 8;
            key |= key << 16;

            offsetEncrypted = entry.offset ^ key;
            sizeEncrypted = entry.size ^ key;

            // 写入偏移和大小
            newLst.seekp(indexOffset);
            newLst.write(reinterpret_cast<char*>(&offsetEncrypted), 4);
            newLst.write(reinterpret_cast<char*>(&sizeEncrypted), 4);

            // 复制原始文件名 - 直接从原始LST文件复制
            oldLst.seekg(indexOffset + 8);
            std::vector<char> nameBuffer(0x40);
            oldLst.read(nameBuffer.data(), 0x40);
            newLst.seekp(indexOffset + 8);
            newLst.write(nameBuffer.data(), 0x40);

            // 复制类型信息
            oldLst.seekg(indexOffset + 0x48);
            int32_t type;
            oldLst.read(reinterpret_cast<char*>(&type), 4);
            newLst.seekp(indexOffset + 0x48);
            newLst.write(reinterpret_cast<char*>(&type), 4);

            indexOffset += 0x4c;
        }
    }

    // 写入封包文件数据
    for (const auto& entry : info.entries) {
        // 如果需要加密
        if (entry.key != 0) {
            std::vector<char> encryptedData = entry.data;
            for (size_t i = 0; i < encryptedData.size(); ++i) {
                encryptedData[i] ^= entry.key;
            }
            newArc.write(encryptedData.data(), encryptedData.size());
        }
        else {
            newArc.write(entry.data.data(), entry.data.size());
        }
    }
}

// 解包功能
void extractPackage(const fs::path& arcFilePath, const fs::path& lstFilePath, const fs::path& outputDir) {
    // 检查文件是否存在
    if (!fs::exists(arcFilePath)) {
        throw std::runtime_error(std::format("Original package file not found: {}", wide2Ascii(arcFilePath)));
    }

    if (!fs::exists(lstFilePath)) {
        throw std::runtime_error(std::format("Original LST file not found: {}", wide2Ascii(lstFilePath)));
    }

    // 打开文件
    std::ifstream arc(arcFilePath, std::ios::binary);

    if (!arc) {
        throw std::runtime_error(std::format("Cannot open file: {}", wide2Ascii(arcFilePath)));
    }

    PackInfo packInfo = analyzePackage(arcFilePath, lstFilePath);

    if (packInfo.format == PackFormat::Unknown) {
        throw std::runtime_error("Cannot recognize package format");
    }

    std::string formatName = (packInfo.format == PackFormat::Moon) ? "Moon" : "Nexton";
    std::println("Detected {} format, key: {:#x}, file count: {}", formatName, packInfo.key, packInfo.entries.size());

    // 保存密钥信息到文本文件
    std::ofstream keyInfo(L"keyInfo.txt");
    keyInfo << std::format("File format: {}\n"
        "Detected key: {:#x}\n"
        "File count: {}\n", formatName, packInfo.key, packInfo.entries.size());
    keyInfo.close();

    // 提取文件
    for (const auto& entry : packInfo.entries) {
        extractFile(arc, entry, outputDir);
    }

    std::println("Extracting completed.");
}

// 重打包功能
void repackPackage(const fs::path& arcFilePath, const fs::path& lstFilePath,
    const fs::path& replaceDir, const fs::path& outputPrefix) {
    // 检查文件是否存在
    if (!fs::exists(arcFilePath)) {
        throw std::runtime_error(std::format("Original package file not found: {}", wide2Ascii(arcFilePath)));
    }

    if (!fs::exists(lstFilePath)) {
        throw std::runtime_error(std::format("Original LST file not found: {}", wide2Ascii(lstFilePath)));
    }

    if (!fs::exists(replaceDir) || !fs::is_directory(replaceDir)) {
        throw std::runtime_error(std::format("Replace directory not found: {}", wide2Ascii(replaceDir)));
    }

    const fs::path newArcFilePath = outputPrefix;
    const fs::path newLstFilePath = outputPrefix.wstring() + L".lst";

    PackInfo packInfo = analyzePackage(arcFilePath, lstFilePath);

    if (packInfo.format == PackFormat::Unknown) {
        throw std::runtime_error("Cannot recognize package format");
    }

    std::string formatName = (packInfo.format == PackFormat::Moon) ? "Moon" : "Nexton";
    std::println("Detected {} format, key: {:#x}, file count: {}", formatName, packInfo.key, packInfo.entries.size());

    // 打开原始封包文件
    std::ifstream arc(arcFilePath, std::ios::binary);
    if (!arc) {
        throw std::runtime_error(std::format("Cannot open file: {}", wide2Ascii(arcFilePath)));
    }

    // 读取原始数据并查找替换文件
    int replacedCount = 0;
    for (auto& entry : packInfo.entries) {
        // 读取原始数据
        readOriginalData(arc, entry);
        // 查找替换文件
        if (findReplaceFile(replaceDir, entry)) {
            std::println("Replaced: {} ({} bytes)", entry.fileName, entry.size);
            replacedCount++;
        }
    }

    std::println("Replaced {} files.", replacedCount);

    // 创建新的封包和LST文件
    std::println("Creating new package...");
    createNewPackage(packInfo, arcFilePath, lstFilePath, newArcFilePath, newLstFilePath);
    std::println("New package created.");
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2024.11.16\n"
        "Usage:\n"
        " For extract: {0} extract arc_file outputDir [lst_file]\n"
        " For repack: {0} repack orig_arc_file orig_lst_file replaceDir outputPrefix\n"
        " Example:\n"
        " {0} extract lcsebody extracted_files_dir\n"
        " {0} repack lcsebody lcsebody.lst new_files_dir lcsebody_new\n", wide2Ascii(programPath.filename()));
}

int wmain(int argc, wchar_t* argv[]) {

    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::wstring mode = argv[1];

    try {
        if (mode == L"extract") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }

            const fs::path arcFilePath = argv[2];
            const fs::path outputDir = argv[3];
            fs::path lstFilePath;

            if (argc >= 5) {
                lstFilePath = argv[4];
            }
            else {
                lstFilePath = arcFilePath.wstring() + L".lst";
            }
            extractPackage(arcFilePath, lstFilePath, outputDir);
            return 0;
        }
        else if (mode == L"repack") {
            if (argc < 6) {
                printUsage(argv[0]);
                return 1;
            }

            const fs::path arcFilePath = argv[2];
            const fs::path lstFilePath = argv[3];
            const fs::path replaceDir = argv[4];
            const fs::path outputPrefix = argv[5];

            repackPackage(arcFilePath, lstFilePath, replaceDir, outputPrefix);
            return 0;
        }
        else {
            std::println("Unknown mode: {}", wide2Ascii(mode, CP_UTF8));
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::println("Error: {}", e.what());
        return 1;
    }

    return 0;
}
