#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>

namespace fs = std::filesystem;

struct Entry {
    char name[32] = { 0 };        // 文件名
    uint32_t size;        // 文件大小
    uint32_t offset;      // 文件偏移
};

void xor_decrypt(std::vector<uint8_t>& data, uint8_t key) {
    for (auto& byte : data) {
        byte ^= key;
    }
}

bool pack_kar(fs::path input_dir, fs::path output_kar) {

    uint32_t signature = 0x52414B;
    uint32_t file_count = 0;
    std::vector<Entry> entries;
    uint32_t CurOffset = 0xc;
    uint32_t padding = 0xc; //offset begin?

    for (auto entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            file_count++;
            CurOffset += sizeof(Entry);
        }
    }

    for (auto entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            Entry fentry;
            std::string filename = entry.path().filename().string();
            memcpy(fentry.name, &filename[0], filename.length());
            fentry.size = entry.file_size();
            fentry.offset = CurOffset;
            entries.push_back(fentry);
            CurOffset += entry.file_size();
        }
    }

    std::ofstream outKar(output_kar, std::ios::binary);
    if (!outKar) {
        std::cout << "Fail to create output_kar file" << std::endl;
        return false;
    }
    outKar.write((char*)&signature, 4);
    outKar.write((char*)&file_count, 4);
    outKar.write((char*)&padding, 4);

    for (auto& entry : entries) {
        outKar.write(entry.name, 32);
        outKar.write((char*)&entry.size, 4);
        outKar.write((char*)&entry.offset, 4);
    }

    for (auto& entry : entries) {
        std::string filename(entry.name);
        std::cout << "Processing: " << filename << std::endl;
        fs::path input_file = input_dir / filename;
        std::ifstream input(input_file, std::ios::binary);
        auto file_size = fs::file_size(input_file);
        std::vector<uint8_t> file_data(file_size);
        input.read((char*)file_data.data(), file_size);
        input.close();
        if (input_file.extension().string() == ".ns6") {
            uint8_t key = static_cast<uint8_t>(entry.size / 7);
            //std::cout << filename << " key: " << (size_t)key << std::endl;
            xor_decrypt(file_data, key);
        }
        else if (input_file.extension().string() == ".ns5") {
            uint8_t key = static_cast<uint8_t>(entry.size / 13);
            xor_decrypt(file_data, key);
        }
        outKar.write((char*)file_data.data(), file_size);
    }
    outKar.close();
    return true;
}

bool extract_kar(const std::string& kar_path, const std::string& output_dir) {
    std::ifstream file(kar_path, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << kar_path << std::endl;
        return false;
    }

    // 读取文件头
    uint32_t signature;
    file.read(reinterpret_cast<char*>(&signature), 4);
    if (signature != 0x52414B) { // 'KAR'
        std::cerr << "无效的KAR文件格式" << std::endl;
        return false;
    }

    // 读取文件数量
    int32_t count;
    file.read(reinterpret_cast<char*>(&count), 4);
    if (count <= 0 || count > 10000) { // 简单的安全检查
        std::cerr << "文件数量异常: " << count << std::endl;
        return false;
    }

    // 创建输出目录
    fs::create_directories(output_dir);

    // 跳到索引表起始位置（0xC）
    file.seekg(0xC);

    // 读取所有文件条目
    std::vector<Entry> entries(count);
    for (int i = 0; i < count; ++i) {
        file.read(reinterpret_cast<char*>(&entries[i]), sizeof(Entry));
    }

    // 提取每个文件
    for (const auto& entry : entries) {
        std::string filename(entry.name);
        std::string output_path = output_dir + "/" + filename;

        // 读取文件数据
        std::vector<uint8_t> data(entry.size);
        file.seekg(entry.offset);
        file.read(reinterpret_cast<char*>(data.data()), entry.size);

        // 检查是否需要解密
        std::string ext = filename.substr(filename.find_last_of("."));
        if (ext == ".ns6") {
            uint8_t key = static_cast<uint8_t>(entry.size / 7);
            //std::cout << filename << " key: " << (size_t)key << std::endl;
            xor_decrypt(data, key);
        }
        else if (ext == ".ns5") {
            uint8_t key = static_cast<uint8_t>(entry.size / 13);
            xor_decrypt(data, key);
        }

        // 写入文件
        std::ofstream outfile(output_path, std::ios::binary);
        if (!outfile) {
            std::cerr << "无法创建文件: " << output_path << std::endl;
            continue;
        }
        outfile.write(reinterpret_cast<char*>(data.data()), data.size());
        std::cout << "已解包: " << filename << std::endl;
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.03.05" << std::endl;
        std::cout << "Usage: " << argv[0] << " <mode> <input> <output>" << std::endl;
        std::cout << "mode: " << "-e for extract, -p for pack" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    fs::path input_path = argv[2];
    fs::path output_path = argv[3];

    if (mode == "-e") {
        if (extract_kar(input_path.string(), output_path.string())) {
            std::cout << "解包完成！" << std::endl;
            return 0;
        }
        else {
            std::cerr << "解包失败！" << std::endl;
            return 1;
        }
    }
    else if (mode == "-p") {
        if (pack_kar(input_path, output_path)) {
            std::cout << "封包完成！" << std::endl;
            return 0;
        }
        else {
            std::cerr << "封包失败！" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "not a invaild mode" << std::endl;
        return 1;
    }
}
