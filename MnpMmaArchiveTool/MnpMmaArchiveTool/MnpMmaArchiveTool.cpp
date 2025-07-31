#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <filesystem>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <Windows.h>

#pragma pack(1)

struct MMAHeader {
    char signature[4];
    uint32_t index_table_offset;
    uint32_t a1;
    uint32_t a2;
    uint32_t file_count;
};

struct MMAIndexEntry {
    uint32_t offset;
    uint32_t org_size;
    uint32_t size;
    uint32_t block_size;
    uint32_t flags;
};

#pragma pack()

namespace fs = std::filesystem;

const std::vector<uint8_t> DefaultKey = {
    0x77, 0x2C, 0x6F, 0x7A, 0x71, 0x4F, 0x25, 0x74, 0x6C, 0x28, 0x7A, 0x81, 0x4C, 0x31, 0x81, 0x5B,
    0x77, 0x81, 0x4D, 0x79, 0x29, 0x69, 0x45, 0x6B, 0x79, 0x7A, 0x68, 0x2D, 0x69, 0x66, 0x29, 0x39,
};

inline uint8_t rotate_left(uint8_t value, int shift, bool compress) {
    if (compress) {
        return value;
    }
    shift &= 7;
    if (shift == 0) return value;
    return (value << shift) | (value >> (8 - shift));
}

void encrypt_in_place(std::vector<uint8_t>& data, size_t offset, size_t length, bool compress) {
    if (DefaultKey.empty()) {
        return;
    }

    if (offset + length > data.size()) {
        length = data.size() - offset;
    }

    size_t key_mask = DefaultKey.size() - 1;

    for (size_t i = 0; i < length; ++i) {
        size_t current_pos = offset + i;
        uint8_t rotated_byte = rotate_left(data[current_pos], 3, compress);
        data[current_pos] = rotated_byte ^ DefaultKey[i & key_mask];
    }
}

// 向右循环移位，是解压时 rotate_left 的逆操作
inline uint8_t rotate_right(uint8_t value, int shift, bool encrypt) {
    if (!encrypt) {
        return value;
    }
    shift &= 7;
    if (shift == 0) return value;
    return (value >> shift) | (value << (8 - shift));
}

// 格式常量
const int MIN_MATCH_LENGTH = 3;
const int MAX_MATCH_LENGTH = 34; // (0x1F) + 3
const int MAX_OFFSET = 2048;     // 11 bits for offset -> 2^11

/**
 * @brief 将原始数据压缩为 MMA/MNP 格式的 LZ 压缩流
 * @param raw_data 待压缩的原始数据
 * @param compressed_data 用于存放压缩后数据的字节向量
 * @return 如果压缩成功返回 true
 */
void compress_lz(const std::vector<uint8_t>& raw_data, std::vector<uint8_t>& compressed_data, bool encrypt) {
    compressed_data.clear();
    compressed_data.push_back(0xC0); // 写入魔术字节

    size_t input_pos = 0;

    while (input_pos < raw_data.size()) {
        uint8_t control_byte = 0;
        std::vector<uint8_t> chunk_data;

        // 记住控制字节在输出流中的位置，我们稍后会回来更新它
        size_t control_byte_pos = compressed_data.size();
        compressed_data.push_back(0); // 先放一个占位符

        // 处理一个最多包含8个操作的块
        for (int bit = 0; bit < 8; ++bit) {
            if (input_pos >= raw_data.size()) break;

            // --- 寻找最佳匹配 ---
            int best_match_length = 0;
            int best_match_offset = 0;

            // 定义搜索窗口
            size_t search_start = (input_pos > MAX_OFFSET) ? (input_pos - MAX_OFFSET) : 0;

            // 限制最大可匹配长度
            size_t max_possible_length = std::min<size_t>((size_t)MAX_MATCH_LENGTH, raw_data.size() - input_pos);

            // 从近到远搜索，这样可以优先找到偏移量小的匹配
            for (size_t p = input_pos - 1; p >= search_start && p != (size_t)-1; --p) {
                int current_match_length = 0;
                while (current_match_length < max_possible_length &&
                    raw_data[p + current_match_length] == raw_data[input_pos + current_match_length]) {
                    current_match_length++;
                }

                if (current_match_length > best_match_length) {
                    best_match_length = current_match_length;
                    best_match_offset = input_pos - p;
                }
            }

            // --- 决策：使用引用还是字面量 ---
            if (best_match_length >= MIN_MATCH_LENGTH) {
                // 使用引用
                control_byte |= (1 << (7 - bit)); // 设置控制位为 1

                uint16_t encoded_offset = best_match_offset - 1;
                uint16_t encoded_length = best_match_length - 3;

                uint16_t packed_word = (encoded_offset << 5) | encoded_length;

                chunk_data.push_back(packed_word >> 8);      // 高位字节
                chunk_data.push_back(packed_word & 0xFF);  // 低位字节

                input_pos += best_match_length;
            }
            else {
                // 使用字面量
                // 控制位默认为 0，无需操作 control_byte

                uint8_t rotated_byte = rotate_right(raw_data[input_pos], 5, encrypt);
                chunk_data.push_back(rotated_byte);

                input_pos++;
            }
        }

        // 回填正确的控制字节
        compressed_data[control_byte_pos] = control_byte;
        // 追加这个块的数据
        compressed_data.insert(compressed_data.end(), chunk_data.begin(), chunk_data.end());
    }

}

typedef int(__stdcall* InitializeMME_t)(int);
typedef int(__stdcall* MME_SetArchiveName_t)(const char*, int);
typedef int(__stdcall* MME_GetDataSize_t)(int);
typedef int(__stdcall* MME_GetMemory_t)(int, void*, unsigned int);
typedef int(__stdcall* MME_GetGraphicSize_t)(int, void*, void*);
typedef int(__stdcall* MME_ReserveMemory_t)(int);
typedef int(__stdcall* MME_MakeFile_t)(int, const char*);

void extract(const std::string& mma_file, const std::string& output_dir) {
    HMODULE hArc = LoadLibraryW(L"ARC.dll");
    if (hArc == NULL) {
        std::cout << "Error: failed to load ARC.dll." << std::endl;
        return;
    }

    InitializeMME_t InitializeMME = (InitializeMME_t)GetProcAddress(hArc, "InitializeMME");
    MME_SetArchiveName_t MME_SetArchiveName = (MME_SetArchiveName_t)GetProcAddress(hArc, "MME_SetArchiveName");
    MME_GetDataSize_t MME_GetDataSize = (MME_GetDataSize_t)GetProcAddress(hArc, "MME_GetDataSize");
    MME_GetMemory_t MME_GetMemory = (MME_GetMemory_t)GetProcAddress(hArc, "MME_GetMemory");
    MME_GetGraphicSize_t MME_GetGraphicSize = (MME_GetGraphicSize_t)GetProcAddress(hArc, "MME_GetGraphicSize");
    MME_ReserveMemory_t MME_ReserveMemory = (MME_ReserveMemory_t)GetProcAddress(hArc, "MME_ReserveMemory");
    MME_MakeFile_t MME_MakeFile = (MME_MakeFile_t)GetProcAddress(hArc, "MME_MakeFile");

    InitializeMME(1);
    int ret = MME_SetArchiveName(mma_file.c_str(), 0);

    fs::create_directories(output_dir);

    std::cout << "MME_SetArchiveName returned " << ret << std::endl;
    int count = 0;

    for (int i = 0; ; i++) {
        int size = MME_GetDataSize(i);
        if (size == -1) {
            break;
        }
        count++;
        std::cout << "MME_GetDataSize returned " << size << std::endl;

        std::vector<char> data(size);
        MME_GetMemory(i, data.data(), size);
        std::ofstream ofs(fs::path(output_dir) / (L"data" + std::to_wstring(i) + L".bin"), std::ios::binary);
        ofs.write(data.data(), size);
        ofs.close();
    }

    std::ifstream ifs;
    ifs.open(fs::path(output_dir) / L"data0.bin");
    std::vector<std::string> fileNames;
    std::string line;
    while (std::getline(ifs, line)) {
        fileNames.push_back(line);
    }
    ifs.close();

    for (int i = 0; i < count; i++) {
        if (i >= fileNames.size()) {
            break;
        }
        fs::rename(fs::path(output_dir) / (L"data" + std::to_wstring(i) + L".bin"), fs::path(output_dir) / fs::path(fileNames[i]).filename());
    }

    FreeLibrary(hArc);
}

void repack(const std::string& org_mma_file, const std::string& input_mod_files_dir, const std::string& output_new_mma_file, bool compress, bool encrypt) {
    std::ifstream ifs(org_mma_file, std::ios::binary);

    MMAHeader header;
    ifs.read((char*)&header, sizeof(header));
    ifs.seekg(header.index_table_offset);
    std::vector<MMAIndexEntry> index_table;
    for (int i = 0; i < header.file_count; i++) {
        MMAIndexEntry entry;
        ifs.read((char*)&entry, sizeof(entry));
        index_table.push_back(entry);
    }
    ifs.close();

    HMODULE hArc = LoadLibraryW(L"ARC.dll");
    if (hArc == NULL) {
        std::cout << "Error: failed to load ARC.dll." << std::endl;
        return;
    }
    InitializeMME_t InitializeMME = (InitializeMME_t)GetProcAddress(hArc, "InitializeMME");
    MME_SetArchiveName_t MME_SetArchiveName = (MME_SetArchiveName_t)GetProcAddress(hArc, "MME_SetArchiveName");
    MME_GetDataSize_t MME_GetDataSize = (MME_GetDataSize_t)GetProcAddress(hArc, "MME_GetDataSize");
    MME_GetMemory_t MME_GetMemory = (MME_GetMemory_t)GetProcAddress(hArc, "MME_GetMemory");

    InitializeMME(1);
    int ret = MME_SetArchiveName(org_mma_file.c_str(), 0);
    std::cout << "MME_SetArchiveName returned " << ret << std::endl;
    int size = MME_GetDataSize(0);
    if (size == -1) {
        return;
    }
    std::cout << "MME_GetDataSize returned " << size << std::endl;
    std::vector<char> data(size);
    MME_GetMemory(0, data.data(), size);
    std::ofstream ofs(L"temp.txt", std::ios::binary);
    ofs.write(data.data(), size);
    ofs.close();

    ifs.open(L"temp.txt");
    std::vector<std::string> fileNames;
    std::string line;
    while (std::getline(ifs, line)) {
        fileNames.push_back(line);
    }
    ifs.close();
    fs::remove(L"temp.txt");

    if (fileNames.size() != index_table.size()) {
        std::cout << "Error: file count in index table does not match file count in original mma file." << std::endl;
        return;
    }

    ofs.open(output_new_mma_file, std::ios::binary);
    if (!ofs.is_open()) {
        std::cout << "Error: failed to open output mma file for appending." << std::endl;
        return;
    }
    {
        ifs.open(org_mma_file, std::ios::binary);
        std::vector<char> buffer(1024 * 1024);
        while (ifs.good()) {
            ifs.read(buffer.data(), buffer.size());
            if (ifs.gcount() > 0) {
                ofs.write(buffer.data(), ifs.gcount());
            }
        }
        ifs.close();
    }

    for (int i = 0; i < fileNames.size(); i++) {
        fs::path mod_file_path = fs::path(input_mod_files_dir) / fs::path(fileNames[i]).filename();
        if (!fs::exists(mod_file_path)) {
            continue;
        }

        std::cout << "Repacking file: " << fs::path(fileNames[i]).filename() << std::endl;
        ifs.open(mod_file_path, std::ios::binary);
        std::vector<uint8_t> mod_data(fs::file_size(mod_file_path));
        ifs.read((char*)mod_data.data(), mod_data.size());
        ifs.close();

        std::vector<uint8_t> compressed_data;

        uint32_t new_offset = ofs.tellp();

        if (index_table[i].block_size != 0) {
            ifs.open(org_mma_file, std::ios::binary);
            std::vector<char> org_data(index_table[i].block_size);
            ifs.seekg(index_table[i].offset);
            ifs.read(org_data.data(), org_data.size());
            ifs.close();
            ofs.write(org_data.data(), org_data.size());
        }

        index_table[i].offset = new_offset;
        index_table[i].org_size = mod_data.size();

        if (compress) {
            compress_lz(mod_data, compressed_data, encrypt);
            if (compressed_data.empty()) {
                std::cout << "Error: failed to compress data for file " << fs::path(fileNames[i]).filename() << std::endl;
                continue;
            }
        }
        else {
            compressed_data = std::move(mod_data);
        }

        if (encrypt) {
            encrypt_in_place(compressed_data, 0, compressed_data.size(), compress);
        }

        index_table[i].size = compressed_data.size() + index_table[i].block_size;
        ofs.write((char*)compressed_data.data(), compressed_data.size());
    }

    ofs.seekp(header.index_table_offset);
    for (int i = 0; i < header.file_count; i++) {
        ofs.write((char*)&index_table[i], sizeof(index_table[i]));
    }

    ofs.close();

    FreeLibrary(hArc);
}

int main(int argc, char* argv[]) {

    if (argc < 4) {
        std::cout << "Made by julixian 2025.08.01" << std::endl;
        std::cout << "Usage: \n"
            << "For extract(can not process pictures very well yet): " << argv[0] << " extract <mma_file> <output_dir>\n"
            << "For repack: " << argv[0] << " repack <org_mma_file> <input_mod_files_dir> <output_new_mma_file> [--compress] [--encrypt]" << std::endl;
        std::cout << "IMPORTANT: Ensure that ARC.dll is in the same directory as your working directory." << std::endl;
        std::cout << "--compress: " << "Compress the modified files before repacking, usually needed for script archive." << std::endl;
        std::cout << "--encrypt: " << "F**k Mnp.(If the archive can be extracted by GARbro, it's probably needed to be encrypted.)" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    if (mode!= "extract" && mode!= "repack") {
        std::cout << "Invalid mode: " << mode << std::endl;
        return 1;
    }

    if (mode == "extract") {
        extract(argv[2], argv[3]);
        return 0;
    }
    else if (mode == "repack") {
        if (argc < 5) {
            std::cout << "Invalid arguments for repack mode." << std::endl;
            return 1;
        }
        bool compress = false;
        bool encrypt = false;
        int arg_index = 5;
        while (arg_index < argc) {
            if (std::string(argv[arg_index]) == "--compress") {
                compress = true;
            }
            else if (std::string(argv[arg_index]) == "--encrypt") {
                encrypt = true;
            }
            else {
                std::cout << "Invalid argument: " << argv[arg_index] << std::endl;
                return 1;
            }
            arg_index++;
        }
        repack(argv[2], argv[3], argv[4], compress, encrypt);
        return 0;
    }

    return -1;
}
