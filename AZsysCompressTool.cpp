#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <zlib.h>
#include <thread>
#include <Windows.h>

namespace fs = std::filesystem;

uint32_t signature = 0x1a425341; //4153421A

uint32_t compute_crc32(const uint8_t* data, size_t length) {
    return crc32(0, data, length);
}

uint32_t compute_adler32(const uint8_t* data, size_t length) {
    return adler32(1, data, length);
}

// 解密ASB文件
bool decrypt_asb(const fs::path& input_path, const fs::path& output_path, uint32_t base_key, bool isEncryptType = false) {
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " << input_path << std::endl;
        return false;
    }

    char signature[4];
    input.read(signature, 4);
    /*if (std::memcmp(signature, "ASB", 3) != 0) {
        std::cerr << "无效的ASB文件: " << input_path << std::endl;
        return false;
    }*/

    uint32_t packed_size, unpacked_size;
    input.read(reinterpret_cast<char*>(&packed_size), 4);
    input.read(reinterpret_cast<char*>(&unpacked_size), 4);

    if (isEncryptType) {
        input.seekg(0x10);
    }

    std::vector<uint8_t> encrypted_data(packed_size);
    input.read(reinterpret_cast<char*>(encrypted_data.data()), packed_size);

    uint32_t key;
    if (isEncryptType) {
        key = 0x9E370001 ^ unpacked_size;
    }
    else {
        key = base_key ^ unpacked_size;
        key ^= ((key << 12) | key) << 11;
    }

    uint32_t* encoded = reinterpret_cast<uint32_t*>(encrypted_data.data());
    for (size_t i = 0; i < encrypted_data.size() / 4; ++i) {
        encoded[i] -= key;
    }

    uint32_t stored_checker = *reinterpret_cast<uint32_t*>(encrypted_data.data());
    uint32_t computed_checker;
    if (isEncryptType) {
        computed_checker = compute_adler32(encrypted_data.data() + 4, encrypted_data.size() - 4);
    }
    else {
        computed_checker = compute_crc32(encrypted_data.data() + 4, encrypted_data.size() - 4);
    }
    if (stored_checker != computed_checker) {
        std::cerr << "校验失败: " << input_path << std::endl;
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

    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        std::cerr << "无法创建输出文件: " << output_path << std::endl;
        return false;
    }

    output.write(reinterpret_cast<char*>(decompressed_data.data()), unpacked_size);
    return true;
}

// 加密ASB文件
bool encrypt_asb(const fs::path& input_path, const fs::path& output_path, uint32_t base_key, bool isEncryptType = false) {

    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " << input_path << std::endl;
        return false;
    }

    input.seekg(0, std::ios::end);
    uint32_t unpacked_size = static_cast<uint32_t>(input.tellg());
    input.seekg(0, std::ios::beg);

    std::vector<uint8_t> input_data(unpacked_size);
    input.read(reinterpret_cast<char*>(input_data.data()), unpacked_size);

    uLong compressed_bound = compressBound(unpacked_size);
    std::vector<uint8_t> compressed_data(compressed_bound + 4); // 额外4字节用于CRC32
    uLong compressed_size = compressed_bound;

    if (compress2(compressed_data.data() + 4, &compressed_size,
        input_data.data(), unpacked_size, Z_BEST_COMPRESSION) != Z_OK) {
        std::cerr << "压缩失败: " << input_path << std::endl;
        return false;
    }

    uint32_t checker;
    if (isEncryptType) {
        checker = compute_adler32(compressed_data.data() + 4, compressed_size);
    }
    else {
        checker = compute_crc32(compressed_data.data() + 4, compressed_size);
    }
    *reinterpret_cast<uint32_t*>(compressed_data.data()) = checker;
    
    uint32_t key;
    if (isEncryptType) {
        key = 0x9E370001 ^ unpacked_size;
    }
    else {
        key = base_key ^ unpacked_size;
        key ^= ((key << 12) | key) << 11;
    }

    uint32_t* data_to_encrypt = reinterpret_cast<uint32_t*>(compressed_data.data());
    for (size_t i = 0; i < (compressed_size + 4) / 4; ++i) {
        data_to_encrypt[i] += key;
    }

    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        std::cerr << "无法创建输出文件: " << output_path << std::endl;
        return false;
    }

    output.write((char*)&signature, 4);
    uint32_t total_packed_size = compressed_size + 4;
    output.write(reinterpret_cast<char*>(&total_packed_size), 4);
    output.write(reinterpret_cast<char*>(&unpacked_size), 4);
    if (isEncryptType) {
        uint32_t padding = 0;
        output.write((char*)&padding, 4);
    }
    output.write(reinterpret_cast<char*>(compressed_data.data()), compressed_size + 4);

    return true;
}

bool found_key = false; // 用于标记是否找到正确的密钥
uint32_t FoundKey = 0;

void try_keys_in_range(const std::vector<uint8_t>& encrypted_data, uint32_t unpacked_size, uint32_t start_key, uint32_t end_key, const fs::path& output_path) {
    std::vector<uint8_t> decrypted_data = encrypted_data;

    for (uint32_t base_key = start_key; base_key < end_key && !found_key; ++base_key) {
        uint32_t key = base_key ^ unpacked_size;
        key ^= ((key << 12) | key) << 11;

        uint32_t* encoded = reinterpret_cast<uint32_t*>(decrypted_data.data());
        for (size_t i = 0; i < decrypted_data.size() / 4; ++i) {
            encoded[i] = encoded[i] - key;
        }

        uint32_t stored_crc = *reinterpret_cast<uint32_t*>(decrypted_data.data());
        uint32_t computed_crc = compute_crc32(decrypted_data.data() + 4, decrypted_data.size() - 4);
        if (stored_crc == computed_crc) {
            
            std::vector<uint8_t> decompressed_data(unpacked_size);
            z_stream strm;
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            strm.avail_in = decrypted_data.size() - 4;
            strm.next_in = decrypted_data.data() + 4;
            strm.avail_out = unpacked_size;
            strm.next_out = decompressed_data.data();

            if (inflateInit(&strm) != Z_OK) {
                continue;
            }

            if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
                inflateEnd(&strm);
                continue;
            }

            inflateEnd(&strm);

            std::ofstream output(output_path, std::ios::binary);
            if (!output) {
                std::cerr << "无法创建输出文件: " << output_path << std::endl;
                continue;
            }

            output.write(reinterpret_cast<char*>(decompressed_data.data()), unpacked_size);
            {
                std::cout << "Key Found: 0x" << std::hex << base_key << std::dec << std::endl;
                FoundKey = base_key;
                found_key = true;
                std::ofstream outTxt(L"#Key.txt");
                outTxt << base_key << std::endl;
                outTxt.close();
                wchar_t buffer[MAX_PATH];
                GetCurrentDirectoryW(MAX_PATH, buffer);
                fs::path CurrentDic(buffer);
                std::cout << "Key has been stored to: " << CurrentDic.string() << "\\#Key.txt" << std::endl;
            }
        }

        decrypted_data = encrypted_data;
    }
}

// 使用多线程猜测密钥解密ASB文件
bool decrypt_and_guess_key_multithreaded(const fs::path& input_path, const fs::path& output_path, unsigned int num_threads) {
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " << input_path << std::endl;
        return false;
    }

    char signature[4];
    input.read(signature, 4);

    uint32_t packed_size, unpacked_size;
    input.read(reinterpret_cast<char*>(&packed_size), 4);
    input.read(reinterpret_cast<char*>(&unpacked_size), 4);

    std::vector<uint8_t> encrypted_data(packed_size);
    input.read(reinterpret_cast<char*>(encrypted_data.data()), packed_size);

    uint32_t num_keys = UINT32_MAX;
    uint32_t range_size = num_keys / num_threads;

    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        uint32_t start_key = i * range_size;
        uint32_t end_key = (i == num_threads - 1) ? num_keys : start_key + range_size;
        threads.emplace_back(try_keys_in_range, std::cref(encrypted_data), unpacked_size, start_key, end_key, output_path);
    }

    for (auto& t : threads) {
        t.join();
    }

    return found_key;
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
    if (argc < 4) {
        std::cout << "Made by julixian 2025.03.06" << std::endl;
        std::cout << "Usage: " << argv[0] << " <mode> [<key>] [<signature>] <input_dir> <output_dir>" << std::endl;
        std::cout << "mode: -d/-d2 decrypt, -e/-e2 encrypt, -g guess key and decrpyt" << std::endl;
        std::cout << "key: can be decimal number or hexadecimal number with 0x prefix (only needed in -d or -e mode)" << std::endl;
        std::cout << "signature: a hexadecimal number with 0x prefix, used to define the uint32_t file signature\nIf the original script use \"ASB\\x1a\", fill with 0x1a425341\nIf the original script use \"ASB\\x00\", fill with 0x00425341, etc. (only needed in -e/-e2 mode)" << std::endl;
        std::cout << "-d/-e/-g is for script files in normal AZsys resource archive, -d2/-e2 is for script files in AZsys encrypted resource archive(GAR shows)" << std::endl;
        std::cout << "Example:" << std::endl;
        std::cout << "  " << argv[0] << " -d 123456789 input_folder output_folder" << std::endl;
        std::cout << "  " << argv[0] << " -d2 input_folder output_folder" << std::endl;
        std::cout << "  " << argv[0] << " -e 0x1DE71CB9 0x1a425341 input_folder output_folder" << std::endl;
        std::cout << "  " << argv[0] << " -e2 0x00425341 input_folder output_folder" << std::endl;
        std::cout << "  " << argv[0] << " -g input_folder output_folder" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    fs::path input_dir(argv[argc - 2]);
    fs::path output_dir(argv[argc - 1]);

    if (!fs::exists(input_dir)) {
        std::cerr << "输入文件夹不存在" << std::endl;
        return 1;
    }

    fs::create_directories(output_dir);

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4; // 默认使用 4 线程

    if (mode == "-d" || mode == "-e") {

        uint32_t base_key = parse_key(argv[2]);
        std::cout << "key used: 0x" << std::hex << std::uppercase << base_key << std::dec << std::endl;

        for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
            fs::path relative = fs::relative(entry.path(), input_dir);
            fs::path output_path = output_dir / relative;
            fs::create_directories(output_path.parent_path());

            if (mode == "-d" && entry.is_regular_file()) {
                std::cout << "Decrypting: " << entry.path().filename() << std::endl;
                if (decrypt_asb(entry.path(), output_path, base_key, false)) {
                    std::cout << "Decrypt successfully: " << output_path << std::endl;
                }
            }
            else if (mode == "-e" && entry.is_regular_file()) {
                signature = parse_key(argv[3]);
                std::cout << "Encrypting: " << entry.path().filename() << std::endl;
                if (encrypt_asb(entry.path(), output_path, base_key, false)) {
                    std::cout << "Encrypt successfully: " << output_path << std::endl;
                }
            }
        }
    }
    else if (mode == "-d2" || mode == "-e2") {

        for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
            fs::path relative = fs::relative(entry.path(), input_dir);
            fs::path output_path = output_dir / relative;
            fs::create_directories(output_path.parent_path());

            if (mode == "-d2" && entry.is_regular_file()) {
                std::cout << "Decrypting: " << entry.path().filename() << std::endl;
                if (decrypt_asb(entry.path(), output_path, 0, true)) {
                    std::cout << "Decrypt successfully: " << output_path << std::endl;
                }
            }
            else if (mode == "-e2" && entry.is_regular_file()) {
                signature = parse_key(argv[2]);
                std::cout << "Encrypting: " << entry.path().filename() << std::endl;
                if (encrypt_asb(entry.path(), output_path, 0, true)) {
                    std::cout << "Encrypt successfully: " << output_path << std::endl;
                }
            }
        }
    }
    else if (mode == "-g") {
        std::cout << "Guessing key with Multi-thread, number of threads: " << num_threads << std::endl;
        for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
            if (entry.is_regular_file()) {
                fs::path relative = fs::relative(entry.path(), input_dir);
                fs::path output_path = output_dir / relative;
                if (!found_key) {
                    fs::path smallest_path;
                    uint32_t minsize = UINT32_MAX;
                    for (const auto& entryS : fs::recursive_directory_iterator(input_dir)) {
                        if (entryS.is_regular_file() && entryS.file_size() < minsize) {
                            minsize = entryS.file_size();
                            smallest_path = entryS.path();
                        }
                    }
                    relative = fs::relative(smallest_path, input_dir);
                    output_path = output_dir / relative;
                    fs::create_directories(output_path.parent_path());
                    std::cout << "Trying to decrypt: " << smallest_path.filename().string() << std::endl;
                    if (decrypt_and_guess_key_multithreaded(smallest_path, output_path, num_threads)) {
                        std::cout << "Decrypt successfully: " << output_path << std::endl;
                    }
                    else {
                        std::cout << "Fail to guess key! The script may have changed its encrypting way" << std::endl;
                        return 1;
                    }
                    relative = fs::relative(entry.path(), input_dir);
                    output_path = output_dir / relative;
                    std::cout << "Decrypting: " << entry.path().filename().string() << std::endl;
                    if (decrypt_asb(entry.path(), output_path, FoundKey, false)) {
                        std::cout << "Decrypt successfully: " << output_path << std::endl;
                    }
                }
                else {
                    std::cout << "Decrypting: " << entry.path().filename().string() << std::endl;
                    if (decrypt_asb(entry.path(), output_path, FoundKey, false)) {
                        std::cout << "Decrypt successfully: " << output_path << std::endl;
                    }
                }
            }
        }
    }

    return 0;
}