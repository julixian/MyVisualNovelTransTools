#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <cstdint>
#include <algorithm>

namespace fs = std::filesystem;

// 文件入口结构
struct Entry {
    std::string name;
    int64_t offset;
    uint32_t size;
};

// 常量定义
const std::string ADVSYS_KEY = "ADVSYS";
const std::string INDEX_KEY = "1qaz2wsx3edc4rfv5tgb6yhn7ujm8ik,9ol.0p;/-@:^[]";

// 随机数生成器
class CRuntimeRandomGenerator {
private:
    uint32_t m_seed;

public:
    void SRand(int seed) {
        m_seed = static_cast<uint32_t>(seed);
    }

    int Rand() {
        m_seed = m_seed * 214013u + 2531011u;
        return (static_cast<int>(m_seed >> 16) & 0x7FFF);
    }
};

// AdvSys加密/解密
void AdvSysEncrypt(std::vector<uint8_t>& data) {
    const size_t text_offset = 136000;
    const size_t text_length = data.size() - text_offset;

    for (size_t i = 0; i < text_length; ++i) {
        data[text_offset + i] ^= ADVSYS_KEY[i % ADVSYS_KEY.length()];
    }
}

void AdvSysDecrypt(std::vector<uint8_t>& data) {
    AdvSysEncrypt(data); // 加密和解密操作相同
}

// 解包相关函数
std::vector<uint8_t> DecryptIndex(const std::string& idx_path) {
    std::ifstream idx_file(idx_path, std::ios::binary);
    if (!idx_file) {
        throw std::runtime_error("Cannot open idx file");
    }

    idx_file.seekg(0, std::ios::end);
    size_t idx_size = (size_t)idx_file.tellg() - 4;
    idx_file.seekg(0, std::ios::beg);

    std::vector<uint8_t> input(idx_size);
    idx_file.read(reinterpret_cast<char*>(input.data()), idx_size);

    int32_t seed;
    idx_file.read(reinterpret_cast<char*>(&seed), sizeof(seed));

    std::vector<uint8_t> output(idx_size);
    CRuntimeRandomGenerator rng;
    rng.SRand(seed);

    for (size_t i = 0; i < idx_size; ++i) {
        output[i] = input[i] ^ INDEX_KEY[rng.Rand() % INDEX_KEY.length()];
    }

    return output;
}

std::vector<Entry> ParseIndex(const std::vector<uint8_t>& index_data) {
    std::vector<Entry> entries;
    size_t offset = 0;
    const size_t name_size = 0x14;

    uint32_t first_offset;
    memcpy(&first_offset, &index_data[name_size], sizeof(uint32_t));

    while (offset < index_data.size()) {
        if (index_data[offset] == 0) break;

        Entry entry;
        char name_buffer[0x15] = { 0 };
        memcpy(name_buffer, &index_data[offset], name_size);
        entry.name = name_buffer;
        offset += name_size;

        uint32_t raw_offset;
        memcpy(&raw_offset, &index_data[offset], sizeof(uint32_t));
        entry.offset = raw_offset - first_offset;
        offset += sizeof(uint32_t);

        memcpy(&entry.size, &index_data[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);

        entries.push_back(entry);
    }

    return entries;
}

// 解包主函数
void ExtractFiles(const std::string& pak_path, const std::string& output_dir) {
    fs::create_directories(output_dir);
    std::string idx_path = pak_path.substr(0, pak_path.length() - 4) + ".idx";

    try {
        auto index_data = DecryptIndex(idx_path);
        auto entries = ParseIndex(index_data);

        std::ifstream pak_file(pak_path, std::ios::binary);
        if (!pak_file) {
            throw std::runtime_error("Cannot open pak file");
        }

        for (const auto& entry : entries) {
            std::cout << "Extracting: " << entry.name << std::endl;

            std::vector<uint8_t> file_data(entry.size);
            pak_file.seekg(entry.offset);
            pak_file.read(reinterpret_cast<char*>(file_data.data()), entry.size);

            if (entry.name.ends_with(".dat")) {
                AdvSysDecrypt(file_data);
            }

            std::string output_path = (fs::path(output_dir) / entry.name).string();
            std::ofstream out_file(output_path, std::ios::binary);
            if (!out_file) {
                throw std::runtime_error("Cannot create output file: " + entry.name);
            }
            out_file.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
        }

        std::cout << "Extraction completed successfully!" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// 打包相关类
class PakPacker {
private:
    std::string m_source_idx;
    std::string m_source_folder;
    std::string m_output_path;
    std::vector<uint8_t> m_index_data;
    int32_t m_seed;
    std::vector<Entry> m_entries;

public:
    PakPacker(const std::string& source_idx,
        const std::string& source_folder,
        const std::string& output_path)
        : m_source_idx(source_idx)
        , m_source_folder(source_folder)
        , m_output_path(output_path) {
    }

    bool Pack() {
        try {
            // 读取并解析原始索引
            if (!LoadOriginalIndex()) return false;

            // 创建新的PAK文件
            if (!CreateNewPak()) return false;

            // 更新并保存新的索引文件
            if (!SaveNewIndex()) return false;

            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error during packing: " << e.what() << std::endl;
            return false;
        }
    }

private:
    bool LoadOriginalIndex() {
        // 读取原始idx文件
        std::ifstream idx_file(m_source_idx, std::ios::binary);
        if (!idx_file) {
            std::cerr << "Cannot open original idx file" << std::endl;
            return false;
        }

        // 获取文件大小
        idx_file.seekg(0, std::ios::end);
        size_t total_size = idx_file.tellg();
        idx_file.seekg(0, std::ios::beg);

        // 读取加密的索引数据
        m_index_data.resize(total_size - 4);
        idx_file.read(reinterpret_cast<char*>(m_index_data.data()), total_size - 4);

        // 读取种子
        idx_file.read(reinterpret_cast<char*>(&m_seed), sizeof(m_seed));

        // 解密索引数据
        DecryptIndex();

        // 解析条目
        if (!ParseEntries()) return false;

        return true;
    }

    void DecryptIndex() {
        std::vector<uint8_t> decrypted(m_index_data.size());
        CRuntimeRandomGenerator rng;
        rng.SRand(m_seed);

        for (size_t i = 0; i < m_index_data.size(); ++i) {
            decrypted[i] = m_index_data[i] ^ INDEX_KEY[rng.Rand() % INDEX_KEY.length()];
        }

        m_index_data = std::move(decrypted);
    }

    bool ParseEntries() {
        size_t offset = 0;
        const size_t entry_size = 0x1C;  // 28 bytes per entry

        while (offset < m_index_data.size() - entry_size) {
            if (m_index_data[offset] == 0) break;

            Entry entry;

            // 读取文件名
            char filename[0x15] = { 0 };
            memcpy(filename, &m_index_data[offset], 0x14);
            entry.name = filename;

            // 读取偏移量和大小
            memcpy(&entry.offset, &m_index_data[offset + 0x14], sizeof(uint32_t));
            memcpy(&entry.size, &m_index_data[offset + 0x18], sizeof(uint32_t));

            m_entries.push_back(entry);
            offset += entry_size;
        }

        return !m_entries.empty();
    }

    bool CreateNewPak() {
        std::ofstream pak_file(m_output_path + ".pak", std::ios::binary);
        if (!pak_file) return false;

        // 首先获取第一个文件的原始偏移量作为基准
        uint32_t base_offset = 0;
        if (!m_entries.empty()) {
            base_offset = static_cast<uint32_t>(m_entries[0].offset);
        }

        uint32_t current_offset = base_offset;  // 使用基准偏移量作为起始点
        const size_t entry_size = 0x1C;
        size_t index_offset = 0;

        // 首先计算所有文件的新大小
        std::vector<std::pair<size_t, uint32_t>> new_sizes;
        for (const auto& entry : m_entries) {
            fs::path source_path = fs::path(m_source_folder) / entry.name;
            if (!fs::exists(source_path)) {
                std::cerr << "Warning: File not found: " << source_path << std::endl;
                new_sizes.push_back({ 0, 0 });
                continue;
            }
            uint32_t file_size = static_cast<uint32_t>(fs::file_size(source_path));
            new_sizes.push_back({ file_size, current_offset });
            current_offset += file_size;
        }

        // 重置current_offset用于实际写入
        current_offset = base_offset;

        // 写入文件并更新索引
        for (size_t i = 0; i < m_entries.size(); ++i) {
            auto& entry = m_entries[i];
            fs::path source_path = fs::path(m_source_folder) / entry.name;

            if (!fs::exists(source_path)) {
                index_offset += entry_size;
                continue;
            }

            // 读取文件数据
            std::vector<uint8_t> file_data = ReadFile(source_path.string());

            // 如果是.dat文件，进行AdvSys加密
            if (entry.name.ends_with(".dat")) {
                AdvSysEncrypt(file_data);
            }

            // 更新索引中的偏移量和大小
            uint32_t new_offset = new_sizes[i].second;
            uint32_t new_size = static_cast<uint32_t>(new_sizes[i].first);

            // 更新索引数据
            memcpy(&m_index_data[index_offset + 0x14], &new_offset, sizeof(uint32_t));
            memcpy(&m_index_data[index_offset + 0x18], &new_size, sizeof(uint32_t));

            // 写入文件数据
            pak_file.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());

            current_offset += file_data.size();
            index_offset += entry_size;
        }

        return true;
    }

    bool SaveNewIndex() {
        // 加密索引数据
        EncryptIndex();

        // 保存新的索引文件
        std::ofstream idx_file(m_output_path + ".idx", std::ios::binary);
        if (!idx_file) return false;

        // 写入加密的索引数据
        idx_file.write(reinterpret_cast<const char*>(m_index_data.data()), m_index_data.size());

        // 写入种子
        idx_file.write(reinterpret_cast<const char*>(&m_seed), sizeof(m_seed));

        return true;
    }

    void EncryptIndex() {
        std::vector<uint8_t> encrypted(m_index_data.size());
        CRuntimeRandomGenerator rng;
        rng.SRand(m_seed);

        for (size_t i = 0; i < m_index_data.size(); ++i) {
            encrypted[i] = m_index_data[i] ^ INDEX_KEY[rng.Rand() % INDEX_KEY.length()];
        }

        m_index_data = std::move(encrypted);
    }

    std::vector<uint8_t> ReadFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open file: " + path);

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);

        return data;
    }
};

// 主函数
void PrintUsage(const char* program_name) {
    std::cout << "Made by julixian 2025.02.17" << std::endl;
    std::cout << "Usage:\n";
    std::cout << "Extract: " << program_name << " -e <pak_file> <output_dir>\n";
    std::cout << "Pack:    " << program_name << " -p <source_idx> <source_folder> <output_path>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "-e") {
        if (argc != 4) {
            PrintUsage(argv[0]);
            return 1;
        }
        ExtractFiles(argv[2], argv[3]);
    }
    else if (mode == "-p") {
        if (argc != 5) {
            PrintUsage(argv[0]);
            return 1;
        }
        PakPacker packer(argv[2], argv[3], argv[4]);
        if (packer.Pack()) {
            std::cout << "Successfully created new pak and idx files!" << std::endl;
            return 0;
        }
        else {
            std::cerr << "Failed to create pak and idx files." << std::endl;
            return 1;
        }
    }
    else {
        PrintUsage(argv[0]);
        return 1;
    }

    return 0;
}
