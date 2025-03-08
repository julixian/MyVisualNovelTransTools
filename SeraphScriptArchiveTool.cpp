#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <zlib.h>
#include <memory>
#include <cstring>

// LZ解压函数
std::vector<uint8_t> lz_decompress(const std::vector<uint8_t>& input) {
    // 读取解压后的大小
    if (input.size() < 4) throw std::runtime_error("Invalid LZ data");
    uint32_t unpacked_size = *(uint32_t*)(input.data());

    std::vector<uint8_t> output(unpacked_size);
    size_t src_pos = 4;  // 跳过大小值
    size_t dst_pos = 0;

    while (dst_pos < unpacked_size) {
        if (src_pos >= input.size()) throw std::runtime_error("Unexpected end of input");

        uint8_t ctl = input[src_pos++];

        if (ctl & 0x80) {  // 复制之前的数据
            if (src_pos >= input.size()) throw std::runtime_error("Unexpected end of input");
            uint8_t lo = input[src_pos++];
            int offset = (((ctl << 3) | (lo >> 5)) & 0x3FF) + 1;
            int count = (lo & 0x1F) + 1;

            if (dst_pos < offset) throw std::runtime_error("Invalid LZ offset");

            // 复制重叠数据
            for (int i = 0; i < count; i++) {
                output[dst_pos] = output[dst_pos - offset];
                dst_pos++;
            }
        }
        else {  // 直接复制数据
            int count = ctl + 1;
            if (src_pos + count > input.size()) throw std::runtime_error("Unexpected end of input");

            memcpy(&output[dst_pos], &input[src_pos], count);
            src_pos += count;
            dst_pos += count;
        }
    }

    return output;
}

// Zlib解压函数
std::vector<uint8_t> decompress_zlib(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    z_stream strm = { nullptr };

    // 初始化zlib
    if (inflateInit(&strm) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib");
    }

    // 设置输入
    strm.next_in = const_cast<Bytef*>(input.data());
    strm.avail_in = static_cast<uInt>(input.size());

    // 分配缓冲区进行解压
    const size_t CHUNK = 16384;
    std::vector<uint8_t> buffer(CHUNK);

    // 解压循环
    do {
        strm.next_out = buffer.data();
        strm.avail_out = static_cast<uInt>(buffer.size());

        int ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            throw std::runtime_error("Decompression failed");
        }

        size_t have = CHUNK - strm.avail_out;
        output.insert(output.end(), buffer.begin(), buffer.begin() + have);
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
    return output;
}

// LZSS伪压缩
std::vector<uint8_t> lz_compress(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    // 写入原始大小
    uint32_t size = static_cast<uint32_t>(input.size());
    output.insert(output.end(), (uint8_t*)&size, (uint8_t*)&size + 4);

    // 每8字节一组进行伪压缩
    for (size_t i = 0; i < input.size(); i += 0x7F) {
        size_t remain = std::min(size_t(0x7F), input.size() - i);
        output.push_back(static_cast<uint8_t>(remain - 1));  // 控制字节
        output.insert(output.end(), input.begin() + i, input.begin() + i + remain);
    }
    return output;
}

// Zlib压缩
std::vector<uint8_t> compress_zlib(const std::vector<uint8_t>& input) {
    uLongf compress_buf_size = compressBound(input.size());
    std::vector<uint8_t> output(compress_buf_size);

    if (compress2(output.data(), &compress_buf_size, input.data(), input.size(),
        Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Zlib compression failed");
    }

    output.resize(compress_buf_size);
    return output;
}

// 封包函数
void pack_files(const std::string& input_dir, const std::string& output_file, int version, bool lz) {
    std::vector<std::filesystem::path> files;

    // 收集所有文件并排序
    for (const auto& entry : std::filesystem::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    // 创建输出文件
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to create output file");
    }

    // 写入文件数量
    uint32_t count = static_cast<uint32_t>(files.size());
    out.write(reinterpret_cast<char*>(&count), sizeof(count));

    // 预留偏移表空间
    std::vector<uint32_t> offsets(count);
    out.seekp(sizeof(uint32_t) * (count + 1));

    // 处理每个文件
    uint32_t current_offset = sizeof(uint32_t) * (count + 1);

    for (size_t i = 0; i < files.size(); ++i) {
        // 读取输入文件
        std::ifstream in(files[i], std::ios::binary);
        std::vector<uint8_t> data(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());

        // 记录偏移
        offsets[i] = current_offset;

        // 压缩数据
        std::vector<uint8_t> lz_compressed;
        if (lz && data.size() > 1024) {
            lz_compressed = lz_compress(data);
        }
        else {
            lz_compressed = data;
        }

        std::vector<uint8_t> zlib_compressed;
        if (version != 3) {
            zlib_compressed = compress_zlib(lz_compressed);
        }
        else {
            zlib_compressed = lz_compressed;
        }

        // 写入标记和压缩数据
        if (version == 2) {
            uint32_t mark = 1;
            out.write(reinterpret_cast<char*>(&mark), sizeof(mark));
            current_offset += 4;
        }
        out.write(reinterpret_cast<char*>(zlib_compressed.data()), zlib_compressed.size());

        // 更新偏移
        current_offset +=  zlib_compressed.size();

        std::cout << "Packed file " << files[i] << " (Size: " << data.size()
            << " -> " << zlib_compressed.size() << " bytes)" << std::endl;
    }

    // 写回偏移表
    out.seekp(sizeof(uint32_t));
    out.write(reinterpret_cast<char*>(offsets.data()), sizeof(uint32_t) * count);

    std::cout << "Packing complete!" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Made by julixian 2025.03.08" << std::endl;
        std::cout << "Usage: " << std::endl;
        std::cout << "For extract: " << argv[0] << " -e [--lz] <input_file> <output_dir>" << std::endl;
        std::cout << "For pack: " << argv[0] << " -p <version> [--lz] <input_dir> <output_file>" << std::endl;
        std::cout << "--lz: " << "decompress/compress file using lzss method when extracting/packing" << std::endl;
        std::cout << "version: 1/2/3, will show when extracting" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string input_path = argv[argc - 2];
    std::string output_path = argv[argc - 1];

    try {
        if (mode == "-e") {
            // 创建输出目录
            bool lz = std::string(argv[2]) == "--lz";
            std::string version;
            int version1 = 0;
            int version2 = 0;
            int version3 = 0;
            std::filesystem::create_directories(output_path);

            // 打开输入文件
            std::ifstream file(input_path, std::ios::binary);
            if (!file) {
                std::cerr << "Failed to open input file" << std::endl;
                return 1;
            }

            // 读取文件数量
            uint32_t count;
            file.read(reinterpret_cast<char*>(&count), sizeof(count));
            std::cout << "File count: " << count << std::endl;

            // 读取偏移表
            std::vector<uint32_t> offsets(count + 1);
            for (uint32_t i = 0; i < count; ++i) {
                file.read(reinterpret_cast<char*>(&offsets[i]), sizeof(uint32_t));
            }
            // 获取文件大小作为最后一个偏移
            file.seekg(0, std::ios::end);
            offsets[count] = static_cast<uint32_t>(file.tellg());

            // 处理每个文件
            for (uint32_t i = 0; i < count; ++i) {
                // 计算文件大小
                uint32_t size = offsets[i + 1] - offsets[i];

                // 读取文件数据
                std::vector<uint8_t> data(size);
                file.seekg(offsets[i]);
                file.read(reinterpret_cast<char*>(data.data()), size);
                std::vector<uint8_t> zlib_decompressed;
                std::vector<uint8_t> final_data;
                try {

                    if (size >= 4 && *reinterpret_cast<uint32_t*>(data.data()) == 1 &&
                        size >= 5 && data[4] == 0x78) {
                        version2++;
                        std::vector<uint8_t> zlib_data(data.begin() + 4, data.end());
                        zlib_decompressed = decompress_zlib(zlib_data);
                    }
                    else if (data[0] == 0x78) {
                        version1++;
                        zlib_decompressed = decompress_zlib(data);
                    }
                    else {
                        version3++;
                        zlib_decompressed = data;
                    }
                    
                }
                catch (const std::exception& e) {
                    zlib_decompressed = data;
                }
                // 然后进行LZ解压
                if (lz && zlib_decompressed.size() > 1024) {
                    final_data = lz_decompress(zlib_decompressed);
                }
                else {
                    final_data = zlib_decompressed;
                }

                // 创建输出文件名
                std::string output_filename = output_path + "/" +
                    std::string(5 - std::to_string(i).length(), '0') + std::to_string(i);

                // 写入解压后的数据
                std::ofstream output(output_filename, std::ios::binary);
                if (output) {
                    output.write(reinterpret_cast<const char*>(final_data.data()),
                        final_data.size());
                    std::cout << "Extracted and decompressed: " << output_filename
                        << " (Size: " << final_data.size() << " bytes)" << std::endl;
                }
            }
            int tmp = std::max(version1, version2);
            tmp = std::max(version3, tmp);
            if (tmp == version1) {
                version = "1";
            }
            else if (tmp == version2) {
                version = "2";
            }
            else {
                version = "3";
            }

            std::cout << "Extraction complete!" << std::endl;
            std::cout << "version: " << version << std::endl;
        }
        else if (mode == "-p") {
            int version;
            bool lz;
            int argOffset = 2;

            version = std::stol(std::string(argv[argOffset++]));
            lz = std::string(argv[argOffset++]) == "--lz";
            pack_files(input_path, output_path, version, lz);
        }
        else {
            std::cout << "Invalid mode. Use -e for extract or -p for create." << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}