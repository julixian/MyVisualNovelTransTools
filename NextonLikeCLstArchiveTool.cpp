#include <Windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include <cstdint>
#include <memory>
#include <iomanip>
#include <map>
#include <algorithm>

namespace fs = std::filesystem;

// 文件类型对应的扩展名
const std::string TYPE_EXT[] = { "LST", "SNX", "BMP", "PNG", "WAV", "OGG" };

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte(936, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte(936, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring AsciiToWide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

// 条目结构
struct Entry {
    std::string originalName;  // 原始文件名（在LST文件中的名称）
    std::string displayName;   // 显示用的文件名（包含扩展名，用于查找替换文件）
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
std::string ReadName(std::ifstream& file, uint32_t offset, uint32_t size, uint8_t key) {
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
void WriteName(std::ofstream& file, const std::string& name, uint32_t offset, uint32_t size, uint8_t key) {
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
std::vector<Entry> OpenMoon(std::ifstream& lst, uint64_t max_offset) {
    std::vector<Entry> dir;

    // 读取文件头
    lst.seekg(0);
    uint32_t count_encrypted;
    lst.read(reinterpret_cast<char*>(&count_encrypted), 4);

    uint32_t count = count_encrypted ^ 0xcccccccc;

    // 验证条目数量
    if (count <= 0 || (4 + count * 0x2c) > max_offset) {
        return {};
    }

    // 读取每个条目
    dir.reserve(count);
    uint32_t index_offset = 4;

    for (uint32_t i = 0; i < count; ++i) {
        lst.seekg(index_offset);

        uint32_t offset_encrypted, size_encrypted;
        lst.read(reinterpret_cast<char*>(&offset_encrypted), 4);
        lst.read(reinterpret_cast<char*>(&size_encrypted), 4);

        uint32_t offset = offset_encrypted ^ 0xcccccccc;
        uint32_t size = size_encrypted ^ 0xcccccccc;

        // 读取文件名
        std::string name = ReadName(lst, index_offset + 8, 0x24, 0xcc);
        name = WideToAscii(AsciiToWide(name, 932), CP_ACP);

        // 验证
        if (name.empty() || offset + size > max_offset) {
            return {};
        }

        Entry entry;
        entry.originalName = name;  // 保存原始名称
        entry.displayName = name;   // 显示用名称
        entry.offset = offset;
        entry.size = size;
        entry.key = 0;
        entry.typeIndex = -1;
        entry.replaced = false;

        dir.push_back(entry);
        index_offset += 0x2c;
    }

    return dir;
}

// 尝试以Nexton格式打开列表文件
std::vector<Entry> OpenNexton(std::ifstream& lst, uint64_t max_offset) {
    std::vector<Entry> dir;

    // 猜测XOR密钥
    lst.seekg(3);
    uint8_t key_byte;
    lst.read(reinterpret_cast<char*>(&key_byte), 1);

    if (key_byte == 0) {
        return {};
    }

    uint32_t key = key_byte;
    key |= key << 8;
    key |= key << 16;

    // 读取文件头
    lst.seekg(0);
    uint32_t count_encrypted;
    lst.read(reinterpret_cast<char*>(&count_encrypted), 4);

    uint32_t count = count_encrypted ^ key;

    // 验证条目数量
    if (count <= 0 || (4 + count * 0x4c) > max_offset) {
        return {};
    }

    // 读取每个条目
    dir.reserve(count);
    uint32_t index_offset = 4;

    for (uint32_t i = 0; i < count; ++i) {
        lst.seekg(index_offset);

        uint32_t offset_encrypted, size_encrypted;
        lst.read(reinterpret_cast<char*>(&offset_encrypted), 4);
        lst.read(reinterpret_cast<char*>(&size_encrypted), 4);

        uint32_t offset = offset_encrypted ^ key;
        uint32_t size = size_encrypted ^ key;

        // 读取文件名
        std::string name = ReadName(lst, index_offset + 8, 0x40, key_byte);
        name = WideToAscii(AsciiToWide(name, 932), CP_ACP);

        // 验证
        if (name.empty() || offset + size > max_offset) {
            return {};
        }

        Entry entry;
        entry.originalName = name;  // 保存原始名称
        entry.displayName = name;   // 显示用名称
        entry.offset = offset;
        entry.size = size;
        entry.key = 0;
        entry.typeIndex = -1;
        entry.replaced = false;

        // 读取类型
        lst.seekg(index_offset + 0x48);
        int32_t type;
        lst.read(reinterpret_cast<char*>(&type), 4);

        if (type >= 0 && type < 6) {
            entry.typeIndex = type;

            // 设置正确的扩展名
            size_t dot_pos = entry.displayName.find_last_of('.');
            if (dot_pos != std::string::npos) {
                entry.displayName = entry.displayName.substr(0, dot_pos);
            }
            entry.displayName += "." + TYPE_EXT[type];

            // 设置文件类型
            if (type == 2 || type == 3) {
                entry.type = "image";
            }
            else if (type == 4 || type == 5) {
                entry.type = "audio";
            }
            else if (type == 1) {
                entry.type = "script";
                entry.key = key_byte + 1;
            }
        }

        dir.push_back(entry);
        index_offset += 0x4c;
    }

    return dir;
}

// 读取文件内容
std::vector<char> ReadFileData(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);

    return buffer;
}

// 获取文件大小
uint64_t GetFileSize(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        return 0;
    }
    return file.tellg();
}

// 读取原始封包中的文件数据
void ReadOriginalData(std::ifstream& arc, Entry& entry) {
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
bool FindReplaceFile(const fs::path& replace_dir, Entry& entry) {
    // 尝试查找完全匹配的文件名
    fs::path replace_path = replace_dir / entry.displayName;
    if (fs::exists(replace_path) && fs::is_regular_file(replace_path)) {
        std::vector<char> new_data = ReadFileData(replace_path.string());
        if (!new_data.empty()) {
            entry.data = std::move(new_data);
            entry.size = static_cast<uint32_t>(entry.data.size());
            entry.replaced = true;
            return true;
        }
    }

    return false;
}

// 提取文件
void ExtractFile(std::ifstream& arc, const Entry& entry, const fs::path& output_dir) {
    // 创建输出目录
    fs::create_directories(output_dir);

    // 打开输出文件
    fs::path output_path = output_dir / entry.displayName;
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "无法创建文件: " << output_path << std::endl;
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
    out.write(buffer.data(), buffer.size());
    std::cout << "已提取: " << entry.displayName << " (" << entry.size << " 字节)" << std::endl;
}

// 分析封包格式
PackInfo AnalyzePackage(const std::string& arc_filename, const std::string& lst_filename) {
    PackInfo info;
    info.format = PackFormat::Unknown;

    // 打开文件
    std::ifstream arc(arc_filename, std::ios::binary);
    std::ifstream lst(lst_filename, std::ios::binary);

    if (!arc || !lst) {
        std::cerr << "错误: 无法打开文件" << std::endl;
        return info;
    }

    // 获取文件大小
    uint64_t max_offset = GetFileSize(arc_filename);
    uint64_t lst_size = GetFileSize(lst_filename);

    // 先尝试Moon格式
    info.entries = OpenMoon(lst, max_offset);
    if (!info.entries.empty()) {
        info.format = PackFormat::Moon;
        info.key = 0xcc;
        info.entrySize = 0x2c;
        return info;
    }

    // 再尝试Nexton格式
    info.entries = OpenNexton(lst, max_offset);
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

// 创建新的封包和LST文件
bool CreateNewPackage(PackInfo& info, const std::string& arc_filename,
    const std::string& lst_filename,
    const std::string& new_arc_filename,
    const std::string& new_lst_filename) {
    // 打开原始LST文件进行读取
    std::ifstream old_lst(lst_filename, std::ios::binary);
    if (!old_lst) {
        std::cerr << "错误: 无法打开原始LST文件" << std::endl;
        return false;
    }

    // 创建新文件
    std::ofstream new_arc(new_arc_filename, std::ios::binary);
    std::ofstream new_lst(new_lst_filename, std::ios::binary);

    if (!new_arc || !new_lst) {
        std::cerr << "错误: 无法创建新文件" << std::endl;
        return false;
    }

    // 写入LST文件头
    uint32_t count = static_cast<uint32_t>(info.entries.size());
    uint32_t count_encrypted;

    if (info.format == PackFormat::Moon) {
        count_encrypted = count ^ 0xcccccccc;
    }
    else {
        uint32_t key = info.key;
        key |= key << 8;
        key |= key << 16;
        count_encrypted = count ^ key;
    }

    new_lst.write(reinterpret_cast<char*>(&count_encrypted), 4);

    // 计算新的文件偏移
    uint32_t current_offset = 0;
    for (auto& entry : info.entries) {
        entry.offset = current_offset;
        current_offset += entry.size;
    }

    // 复制原始LST文件内容并更新偏移和大小
    std::vector<char> lst_buffer(GetFileSize(lst_filename));
    old_lst.seekg(0);
    old_lst.read(lst_buffer.data(), lst_buffer.size());

    // 写入LST文件条目
    uint32_t index_offset = 4;
    for (const auto& entry : info.entries) {
        uint32_t offset_encrypted, size_encrypted;

        if (info.format == PackFormat::Moon) {
            offset_encrypted = entry.offset ^ 0xcccccccc;
            size_encrypted = entry.size ^ 0xcccccccc;

            // 写入偏移和大小
            new_lst.seekp(index_offset);
            new_lst.write(reinterpret_cast<char*>(&offset_encrypted), 4);
            new_lst.write(reinterpret_cast<char*>(&size_encrypted), 4);

            // 复制原始文件名 - 直接从原始LST文件复制
            old_lst.seekg(index_offset + 8);
            std::vector<char> name_buffer(0x24);
            old_lst.read(name_buffer.data(), 0x24);
            new_lst.seekp(index_offset + 8);
            new_lst.write(name_buffer.data(), 0x24);

            index_offset += 0x2c;
        }
        else {
            uint32_t key = info.key;
            key |= key << 8;
            key |= key << 16;

            offset_encrypted = entry.offset ^ key;
            size_encrypted = entry.size ^ key;

            // 写入偏移和大小
            new_lst.seekp(index_offset);
            new_lst.write(reinterpret_cast<char*>(&offset_encrypted), 4);
            new_lst.write(reinterpret_cast<char*>(&size_encrypted), 4);

            // 复制原始文件名 - 直接从原始LST文件复制
            old_lst.seekg(index_offset + 8);
            std::vector<char> name_buffer(0x40);
            old_lst.read(name_buffer.data(), 0x40);
            new_lst.seekp(index_offset + 8);
            new_lst.write(name_buffer.data(), 0x40);

            // 复制类型信息
            old_lst.seekg(index_offset + 0x48);
            int32_t type;
            old_lst.read(reinterpret_cast<char*>(&type), 4);
            new_lst.seekp(index_offset + 0x48);
            new_lst.write(reinterpret_cast<char*>(&type), 4);

            index_offset += 0x4c;
        }
    }

    // 写入封包文件数据
    for (const auto& entry : info.entries) {
        // 如果需要加密
        if (entry.key != 0) {
            std::vector<char> encrypted_data = entry.data;
            for (size_t i = 0; i < encrypted_data.size(); ++i) {
                encrypted_data[i] ^= entry.key;
            }
            new_arc.write(encrypted_data.data(), encrypted_data.size());
        }
        else {
            new_arc.write(entry.data.data(), entry.data.size());
        }
    }

    return true;
}

// 解包功能
bool ExtractPackage(const std::string& arc_filename, const std::string& lst_filename, const std::string& output_dir) {
    // 检查文件是否存在
    if (!fs::exists(arc_filename)) {
        std::cerr << "错误: 资源文件不存在: " << arc_filename << std::endl;
        return false;
    }

    if (!fs::exists(lst_filename)) {
        std::cerr << "错误: LST文件不存在: " << lst_filename << std::endl;
        return false;
    }

    // 打开文件
    std::ifstream arc(arc_filename, std::ios::binary);
    std::ifstream lst(lst_filename, std::ios::binary);

    if (!arc || !lst) {
        std::cerr << "错误: 无法打开文件" << std::endl;
        return false;
    }

    // 获取文件大小
    uint64_t max_offset = GetFileSize(arc_filename);
    uint64_t lst_size = GetFileSize(lst_filename);

    // 尝试解析列表文件
    std::vector<Entry> dir;
    uint8_t detected_key = 0;
    std::string format_type;

    // 先尝试Moon格式
    dir = OpenMoon(lst, max_offset);
    if (!dir.empty()) {
        format_type = "Moon";
        detected_key = 0xcc;
    }
    else {
        // 再尝试Nexton格式
        dir = OpenNexton(lst, max_offset);
        if (!dir.empty()) {
            format_type = "Nexton";
            // 获取猜测的密钥
            lst.seekg(3);
            lst.read(reinterpret_cast<char*>(&detected_key), 1);
        }
    }

    if (dir.empty()) {
        std::cerr << "错误: 无法解析LST文件，不支持的格式或文件已损坏" << std::endl;
        return false;
    }

    // 创建输出目录
    fs::create_directories(output_dir);

    // 保存密钥信息到文本文件
    std::ofstream key_info(fs::path(output_dir) / "key_info.txt");
    key_info << "文件格式: " << format_type << std::endl;
    key_info << "检测到的密钥: 0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(detected_key) << std::endl;
    key_info << "文件总数: " << dir.size() << std::endl;
    key_info.close();

    std::cout << "检测到 " << format_type << " 格式，密钥: 0x" << std::hex << std::setw(2)
        << std::setfill('0') << static_cast<int>(detected_key) << std::endl;
    std::cout << "开始提取 " << dir.size() << " 个文件到 " << output_dir << std::endl;

    // 提取文件
    for (const auto& entry : dir) {
        ExtractFile(arc, entry, output_dir);
    }

    std::cout << "提取完成!" << std::endl;
    return true;
}

// 重打包功能
bool RepackPackage(const std::string& arc_filename, const std::string& lst_filename,
    const std::string& replace_dir, const std::string& output_prefix) {
    // 检查文件是否存在
    if (!fs::exists(arc_filename)) {
        std::cerr << "错误: 原始封包文件不存在: " << arc_filename << std::endl;
        return false;
    }

    if (!fs::exists(lst_filename)) {
        std::cerr << "错误: 原始LST文件不存在: " << lst_filename << std::endl;
        return false;
    }

    if (!fs::exists(replace_dir) || !fs::is_directory(replace_dir)) {
        std::cerr << "错误: 替换文件目录不存在或不是目录: " << replace_dir << std::endl;
        return false;
    }

    std::string new_arc_filename = output_prefix;
    std::string new_lst_filename = output_prefix + ".lst";

    // 分析封包格式
    std::cout << "正在分析封包格式..." << std::endl;
    PackInfo pack_info = AnalyzePackage(arc_filename, lst_filename);

    if (pack_info.format == PackFormat::Unknown) {
        std::cerr << "错误: 无法识别封包格式" << std::endl;
        return false;
    }

    std::string format_name = (pack_info.format == PackFormat::Moon) ? "Moon" : "Nexton";
    std::cout << "检测到 " << format_name << " 格式，密钥: 0x" << std::hex << std::setw(2)
        << std::setfill('0') << static_cast<int>(pack_info.key) << std::endl;
    std::cout << "文件总数: " << pack_info.entries.size() << std::endl;

    // 打开原始封包文件
    std::ifstream arc(arc_filename, std::ios::binary);
    if (!arc) {
        std::cerr << "错误: 无法打开原始封包文件" << std::endl;
        return false;
    }

    // 读取原始数据并查找替换文件
    int replaced_count = 0;
    for (auto& entry : pack_info.entries) {
        // 读取原始数据
        ReadOriginalData(arc, entry);

        // 查找替换文件
        if (FindReplaceFile(replace_dir, entry)) {
            std::cout << "替换文件: " << entry.displayName << " (新大小: " << entry.size << " 字节)" << std::endl;
            replaced_count++;
        }
    }

    std::cout << "共替换 " << replaced_count << " 个文件" << std::endl;

    // 创建新的封包和LST文件
    std::cout << "正在创建新的封包和LST文件..." << std::endl;
    if (CreateNewPackage(pack_info, arc_filename, lst_filename, new_arc_filename, new_lst_filename)) {
        std::cout << "成功创建新封包: " << new_arc_filename << std::endl;
        std::cout << "成功创建新LST文件: " << new_lst_filename << std::endl;
    }
    else {
        std::cerr << "错误: 创建新文件失败" << std::endl;
        return false;
    }

    // 创建替换信息日志
    std::ofstream log_file(output_prefix + "_replace_log.txt");
    if (log_file) {
        log_file << "封包格式: " << format_name << std::endl;
        log_file << "密钥: 0x" << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(pack_info.key) << std::endl;
        log_file << "文件总数: " << pack_info.entries.size() << std::endl;
        log_file << "替换文件数: " << replaced_count << std::endl << std::endl;

        log_file << "替换文件列表:" << std::endl;
        for (const auto& entry : pack_info.entries) {
            if (entry.replaced) {
                log_file << entry.displayName << " (大小: " << entry.size << " 字节)" << std::endl;
            }
        }

        log_file.close();
        std::cout << "已创建替换日志: " << output_prefix << "_replace_log.txt" << std::endl;
    }

    return true;
}

void ShowHelp(const char* programName) {
    std::cout << "Made by julixian 2024.04.11" << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  解包: " << programName << " extract <资源文件> <输出目录> [lst文件]" << std::endl;
    std::cout << "  打包: " << programName << " repack <原始封包> <原始LST文件> <替换文件目录> <输出前缀>" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  解包: " << programName << " extract data extracted_files_dir" << std::endl;
    std::cout << "  打包: " << programName << " repack data data.lst new_files_dir output" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ShowHelp(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "extract") {
        if (argc < 4) {
            std::cout << "用法: " << argv[0] << " extract <资源文件> <输出目录> [lst文件]" << std::endl;
            return 1;
        }

        std::string arc_filename = argv[2];
        std::string output_dir = argv[3];
        std::string lst_filename;

        if (argc >= 5) {
            lst_filename = argv[4];
        }
        else {
            // 尝试查找与资源文件同名的lst文件
            lst_filename = arc_filename + ".lst";
        }

        if (ExtractPackage(arc_filename, lst_filename, output_dir)) {
            return 0;
        }
        else {
            return 1;
        }
    }
    else if (command == "repack") {
        if (argc < 6) {
            std::cout << "用法: " << argv[0] << " repack <原始封包> <原始LST文件> <替换文件目录> <输出前缀>" << std::endl;
            return 1;
        }

        std::string arc_filename = argv[2];
        std::string lst_filename = argv[3];
        std::string replace_dir = argv[4];
        std::string output_prefix = argv[5];

        if (RepackPackage(arc_filename, lst_filename, replace_dir, output_prefix)) {
            return 0;
        }
        else {
            return 1;
        }
    }
    else {
        std::cout << "未知命令: " << command << std::endl;
        ShowHelp(argv[0]);
        return 1;
    }

    return 0;
}
