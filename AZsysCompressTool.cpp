#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <zlib.h>

namespace fs = std::filesystem;

// CRC32计算函数
uint32_t compute_crc32(const uint8_t* data, size_t length) {
    return crc32(0, data, length);
}

// 解密ASB文件
bool decrypt_asb(const fs::path& input_path, const fs::path& output_path, uint32_t base_key) {
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " << input_path << std::endl;
        return false;
    }

    // 读取文件头
    char signature[4];
    input.read(signature, 4);
    if (std::memcmp(signature, "ASB\x1a", 4) != 0) {
        std::cerr << "无效的ASB文件: " << input_path << std::endl;
        return false;
    }

    // 读取大小信息
    uint32_t packed_size, unpacked_size;
    input.read(reinterpret_cast<char*>(&packed_size), 4);
    input.read(reinterpret_cast<char*>(&unpacked_size), 4);

    // 读取加密数据
    std::vector<uint8_t> encrypted_data(packed_size);
    input.read(reinterpret_cast<char*>(encrypted_data.data()), packed_size);

    // 计算解密密钥
    uint32_t key = base_key ^ unpacked_size;
    key ^= ((key << 12) | key) << 11;

    // 解密数据
    uint32_t* encoded = reinterpret_cast<uint32_t*>(encrypted_data.data());
    for (size_t i = 0; i < encrypted_data.size() / 4; ++i) {
        encoded[i] -= key;
    }

    // 验证CRC32
    uint32_t stored_crc = *reinterpret_cast<uint32_t*>(encrypted_data.data());
    uint32_t computed_crc = compute_crc32(encrypted_data.data() + 4, encrypted_data.size() - 4);
    if (stored_crc != computed_crc) {
        std::cerr << "CRC32校验失败: " << input_path << std::endl;
        return false;
    }

    // zlib解压缩
    std::vector<uint8_t> decompressed_data(unpacked_size);
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = encrypted_data.size() - 4;
    strm.next_in = encrypted_data.data() + 4;
    strm.avail_out = unpacked_size;
    strm.next_out = decompressed_data.data();

    if (inflateInit(&strm) != Z_OK) {
        std::cerr << "zlib初始化失败: " << input_path << std::endl;
        return false;
    }

    if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
        inflateEnd(&strm);
        std::cerr << "解压缩失败: " << input_path << std::endl;
        return false;
    }

    inflateEnd(&strm);

    // 写入解密后的文件
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        std::cerr << "无法创建输出文件: " << output_path << std::endl;
        return false;
    }

    output.write(reinterpret_cast<char*>(decompressed_data.data()), unpacked_size);
    return true;
}

// 加密ASB文件
bool encrypt_asb(const fs::path& input_path, const fs::path& output_path, uint32_t base_key) {
    // 读取输入文件
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " << input_path << std::endl;
        return false;
    }

    // 获取文件大小
    input.seekg(0, std::ios::end);
    uint32_t unpacked_size = static_cast<uint32_t>(input.tellg());
    input.seekg(0, std::ios::beg);

    // 读取文件内容
    std::vector<uint8_t> input_data(unpacked_size);
    input.read(reinterpret_cast<char*>(input_data.data()), unpacked_size);

    // 压缩数据
    uLong compressed_bound = compressBound(unpacked_size);
    std::vector<uint8_t> compressed_data(compressed_bound + 4); // 额外4字节用于CRC32
    uLong compressed_size = compressed_bound;

    if (compress2(compressed_data.data() + 4, &compressed_size,
        input_data.data(), unpacked_size, Z_BEST_COMPRESSION) != Z_OK) {
        std::cerr << "压缩失败: " << input_path << std::endl;
        return false;
    }

    // 计算并存储CRC32
    uint32_t crc = compute_crc32(compressed_data.data() + 4, compressed_size);
    *reinterpret_cast<uint32_t*>(compressed_data.data()) = crc;

    // 计算加密密钥
    uint32_t key = base_key ^ unpacked_size;
    key ^= ((key << 12) | key) << 11;

    // 加密数据
    uint32_t* data_to_encrypt = reinterpret_cast<uint32_t*>(compressed_data.data());
    for (size_t i = 0; i < (compressed_size + 4) / 4; ++i) {
        data_to_encrypt[i] += key;
    }

    // 写入输出文件
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        std::cerr << "无法创建输出文件: " << output_path << std::endl;
        return false;
    }

    // 写入头部信息
    output.write("ASB\x1a", 4);
    uint32_t total_packed_size = compressed_size + 4;
    output.write(reinterpret_cast<char*>(&total_packed_size), 4);
    output.write(reinterpret_cast<char*>(&unpacked_size), 4);

    // 写入加密后的数据
    output.write(reinterpret_cast<char*>(compressed_data.data()), compressed_size + 4);

    return true;
}

// 解析密钥字符串为uint32_t
uint32_t parse_key(const std::string& key_str) {
    try {
        if (key_str.substr(0, 2) == "0x" || key_str.substr(0, 2) == "0X") {
            // 16进制输入
            return std::stoul(key_str.substr(2), nullptr, 16);
        }
        else {
            // 10进制输入
            return std::stoul(key_str);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "无效的密钥格式！请使用10进制数字或以0x开头的16进制数字" << std::endl;
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cout << "Made by julixian 2025.01.12" << std::endl;
        std::cout << "用法: " << argv[0] << " <模式> <密钥> <输入文件夹> <输出文件夹>" << std::endl;
        std::cout << "模式: -d 解密, -e 加密" << std::endl;
        std::cout << "密钥: 可以是10进制数字或以0x开头的16进制数字" << std::endl;
        std::cout << "示例:" << std::endl;
        std::cout << "  " << argv[0] << " -d 123456789 input_folder output_folder" << std::endl;
        std::cout << "  " << argv[0] << " -e 0x1DE71CB9 input_folder output_folder" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    uint32_t base_key = parse_key(argv[2]);
    fs::path input_dir(argv[3]);
    fs::path output_dir(argv[4]);

    if (!fs::exists(input_dir)) {
        std::cerr << "输入文件夹不存在" << std::endl;
        return 1;
    }

    std::cout << "使用密钥: 0x" << std::hex << std::uppercase << base_key << std::dec << std::endl;

    // 创建输出文件夹
    fs::create_directories(output_dir);

    // 遍历输入文件夹
    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (mode == "-d" && entry.path().extension() == ".asb") {
            // 解密模式
            fs::path relative = fs::relative(entry.path(), input_dir);
            fs::path output_path = output_dir / relative;
            fs::create_directories(output_path.parent_path());

            std::cout << "正在解密: " << entry.path().filename() << std::endl;
            if (decrypt_asb(entry.path(), output_path, base_key)) {
                std::cout << "解密成功: " << output_path << std::endl;
            }
        }
        else if (mode == "-e") {
            // 加密模式
            fs::path relative = fs::relative(entry.path(), input_dir);
            fs::path output_path = output_dir / relative;
            output_path.replace_extension(".asb");
            fs::create_directories(output_path.parent_path());

            std::cout << "正在加密: " << entry.path().filename() << std::endl;
            if (encrypt_asb(entry.path(), output_path, base_key)) {
                std::cout << "加密成功: " << output_path << std::endl;
            }
        }
    }

    return 0;
}
