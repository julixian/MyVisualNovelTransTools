#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <filesystem> // C++17 标准库，用于文件和目录操作
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <map>
#include <cstring> // 用于 memcpy

// 为了跨平台兼容性，定义文件系统库的别名
namespace fs = std::filesystem;

// =================================================================
// 二进制数据读写辅助函数
// =================================================================

// 读取小端序 32 位无符号整数
uint32_t read_u32_le(std::istream& is) {
    uint32_t value;
    is.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

// 读取大端序 32 位无符号整数
uint32_t read_u32_be(std::istream& is) {
    uint8_t bytes[4];
    is.read(reinterpret_cast<char*>(bytes), 4);
    return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
        (static_cast<uint32_t>(bytes[2]) << 8) | (static_cast<uint32_t>(bytes[3]));
}

// 写入小端序 32 位无符号整数
void write_u32_le(std::ostream& os, uint32_t value) {
    os.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

// 写入大端序 32 位无符号整数
void write_u32_be(std::ostream& os, uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    os.write(reinterpret_cast<const char*>(bytes), 4);
}

// 辅助函数：向内存缓冲区中写入大端序 32 位整数
void write_u32_be_to_buffer(std::vector<char>& buffer, size_t offset, uint32_t value) {
    if (offset + 4 > buffer.size()) throw std::out_of_range("写入缓冲区时发生越界");
    buffer[offset] = (value >> 24) & 0xFF;
    buffer[offset + 1] = (value >> 16) & 0xFF;
    buffer[offset + 2] = (value >> 8) & 0xFF;
    buffer[offset + 3] = value & 0xFF;
}

// =================================================================
// 数据结构定义
// =================================================================
struct FileEntry {
    std::string name;       // 文件名（在封包内的相对路径）
    uint64_t offset;        // 文件数据在封包内的偏移
    uint32_t packed_size;   // 压缩后的大小
    uint32_t unpacked_size; // 解压后的大小
    bool is_packed;         // 是否被压缩
    fs::path source_path;   // 封包时使用的源文件在本地的路径
};

// SHS 解压缩算法
std::vector<uint8_t> decompress_shs(const std::vector<uint8_t>& compressed_data, uint32_t unpacked_size) {
    if (unpacked_size == 0) return {};
    std::vector<uint8_t> output(unpacked_size);
    size_t src_pos = 0, dst_pos = 0;
    auto read_src_u8 = [&]() { return compressed_data.at(src_pos++); };
    auto read_src_u16_be = [&]() { uint16_t val = (static_cast<uint16_t>(compressed_data.at(src_pos)) << 8) | compressed_data.at(src_pos + 1); src_pos += 2; return val; };
    auto read_src_u32_be = [&]() { uint32_t val = (static_cast<uint32_t>(compressed_data.at(src_pos)) << 24) | (static_cast<uint32_t>(compressed_data.at(src_pos + 1)) << 16) | (static_cast<uint32_t>(compressed_data.at(src_pos + 2)) << 8) | compressed_data.at(src_pos + 3); src_pos += 4; return val; };
    while (dst_pos < unpacked_size) {
        if (src_pos >= compressed_data.size()) throw std::runtime_error("解压错误：压缩数据意外结束。");
        uint8_t ctl = read_src_u8();
        uint32_t count = 0;
        if (ctl < 32) {
            switch (ctl) {
            case 0x1D: count = read_src_u8() + 0x1E; break;
            case 0x1E: count = read_src_u16_be() + 0x11E; break;
            case 0x1F: count = read_src_u32_be(); break;
            default: count = ctl + 1; break;
            }
            count = std::min(count, static_cast<uint32_t>(unpacked_size - dst_pos));
            if (src_pos + count > compressed_data.size()) throw std::runtime_error("解压错误：字面量读取越界。");
            std::copy(compressed_data.begin() + src_pos, compressed_data.begin() + src_pos + count, output.begin() + dst_pos);
            src_pos += count;
        }
        else {
            uint32_t offset = 0;
            if ((ctl & 0x80) == 0) {
                if ((ctl & 0x60) == 0x20) { offset = (ctl >> 2) & 7; count = ctl & 3; }
                else {
                    offset = read_src_u8();
                    if ((ctl & 0x60) == 0x40) count = (ctl & 0x1F) + 4;
                    else {
                        offset |= (ctl & 0x1F) << 8;
                        ctl = read_src_u8();
                        if (ctl == 0xFE) count = read_src_u16_be() + 0x102;
                        else if (ctl == 0xFF) count = read_src_u32_be();
                        else count = ctl + 4;
                    }
                }
            }
            else { count = (ctl >> 5) & 3; offset = ((ctl & 0x1F) << 8) | read_src_u8(); }
            count += 3;
            offset++;
            count = std::min(count, static_cast<uint32_t>(unpacked_size - dst_pos));
            if (dst_pos < offset) throw std::runtime_error("解压错误：无效的回溯引用偏移。");
            for (uint32_t i = 0; i < count; ++i) output[dst_pos + i] = output[dst_pos - offset + i];
        }
        dst_pos += count;
    }
    return output;
}
// 提取封包
void extract_archive(const fs::path& archive_path, const fs::path& output_dir) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) throw std::runtime_error("无法打开封包文件: " + archive_path.string());

    uint32_t signature = read_u32_le(file);
    if (signature != 0x356D6948 && signature != 0x37534853) {
        throw std::runtime_error("提取模式目前仅支持 Him5/SHS7 格式。");
    }

    file.seekg(4);
    uint32_t section_count = read_u32_le(file);
    std::vector<std::pair<uint32_t, uint32_t>> sections;
    for (uint32_t i = 0; i < section_count; ++i) {
        uint32_t size = read_u32_le(file);
        uint32_t offset = read_u32_le(file);
        if (size > 0) sections.push_back({ offset, size });
    }

    std::vector<FileEntry> dir;
    for (const auto& section : sections) {
        file.seekg(section.first);
        uint32_t section_size_left = section.second;
        while (section_size_left > 0) {
            uint8_t entry_size;
            file.read(reinterpret_cast<char*>(&entry_size), 1);
            if (entry_size < 5) break;
            FileEntry entry;
            entry.offset = read_u32_be(file);
            uint32_t name_len = entry_size - 5;
            std::string name(name_len, '\0');
            file.read(&name[0], name_len);
            entry.name = name;
            dir.push_back(entry);
            section_size_left -= entry_size;
        }
    }

    std::cout << "发现 " << dir.size() << " 个文件。正在创建输出目录..." << std::endl;
    fs::create_directories(output_dir);
    for (auto& entry : dir) {
        std::cout << "正在提取: " << entry.name << "... ";
        file.seekg(entry.offset);
        entry.packed_size = read_u32_le(file);
        entry.unpacked_size = read_u32_le(file);
        entry.is_packed = (entry.packed_size != 0);
        uint32_t data_size_to_read = entry.is_packed ? entry.packed_size : entry.unpacked_size;
        if (data_size_to_read == 0) { std::cout << " (空文件)" << std::endl; fs::path out_path = output_dir / entry.name; std::ofstream out_file(out_path, std::ios::binary); continue; }
        std::vector<uint8_t> file_data(data_size_to_read);
        file.read(reinterpret_cast<char*>(file_data.data()), data_size_to_read);
        std::vector<uint8_t> final_data;
        if (entry.is_packed) {
            std::cout << "(已压缩, 正在解压) ";
            try { final_data = decompress_shs(file_data, entry.unpacked_size); }
            catch (const std::exception& e) { std::cout << "\n错误：解压 " << entry.name << " 失败: " << e.what() << std::endl; continue; }
        }
        else { std::cout << "(未压缩) "; final_data = std::move(file_data); }
        fs::path out_path = output_dir / entry.name;
        fs::create_directories(out_path.parent_path());
        std::ofstream out_file(out_path, std::ios::binary);
        if (!out_file) { std::cout << "\n错误：无法创建输出文件: " << out_path << std::endl; continue; }
        out_file.write(reinterpret_cast<const char*>(final_data.data()), final_data.size());
        std::cout << "完成。" << std::endl;
    }
    std::cout << "\n提取完成！" << std::endl;
}

void modify_append_archive(const fs::path& original_hxp, const fs::path& replacements_dir, const fs::path& new_hxp) {

    try {
        fs::copy_file(original_hxp, new_hxp, fs::copy_options::overwrite_existing);
        std::cout << "已成功将原始封包复制到: " << new_hxp << std::endl;
    }
    catch (const fs::filesystem_error& e) {
        throw std::runtime_error("无法复制原始封包文件: " + std::string(e.what()));
    }

    std::fstream new_file(new_hxp, std::ios::in | std::ios::out | std::ios::binary);
    if (!new_file) {
        throw std::runtime_error("无法以读写模式打开新封包文件: " + new_hxp.string());
    }

    new_file.seekg(4);
    uint32_t section_count = read_u32_le(new_file);
    if (section_count == 0) {
        std::cout << "封包不包含任何区段，无需修改。" << std::endl;
        return;
    }

    // 读取所有区段的描述信息
    std::vector<std::pair<uint32_t, uint32_t>> section_descriptors;
    for (uint32_t i = 0; i < section_count; ++i) {
        uint32_t size = read_u32_le(new_file);
        uint32_t offset = read_u32_le(new_file);
        section_descriptors.push_back({ offset, size });
    }

    // 存储每个索引块的内存副本，键是区段偏移量
    std::map<uint32_t, std::vector<char>> index_blocks;

    // 存储每个文件条目的信息及其在哪个索引块、哪个位置
    struct IndexPatchInfo {
        std::string name;
        uint32_t section_offset;      // 该条目所属的索引块的偏移
        size_t entry_pos_in_buffer; // 该条目在对应索引缓冲区中的起始位置
    };
    std::vector<IndexPatchInfo> all_patch_infos;

    std::cout << "正在解析所有区段..." << std::endl;
    for (const auto& desc : section_descriptors) {
        uint32_t section_offset = desc.first;
        uint32_t section_size = desc.second;
        if (section_size == 0) continue;

        // 读取整个索引块到内存
        std::vector<char> current_block(section_size);
        new_file.seekg(section_offset);
        new_file.read(current_block.data(), section_size);
        index_blocks[section_offset] = current_block;

        // 解析这个块里的所有文件条目
        size_t current_pos_in_block = 0;
        while (current_pos_in_block < section_size) {
            uint8_t entry_size = static_cast<uint8_t>(current_block[current_pos_in_block]);
            if (entry_size < 5) break;

            uint32_t name_len = entry_size - 5;
            std::string name(&current_block[current_pos_in_block + 5], name_len);

            all_patch_infos.push_back({ name, section_offset, current_pos_in_block });

            current_pos_in_block += entry_size;
        }
    }
    std::cout << "共解析到 " << section_count << " 个区段，" << all_patch_infos.size() << " 个文件条目。" << std::endl;

    // --- 4. 遍历所有文件条目，检查并追加替换文件 ---
    std::vector<char> copy_buffer(1024 * 1024); // 1MB 缓冲区

    for (const auto& info : all_patch_infos) {
        fs::path replacement_path = replacements_dir / info.name;

        if (fs::exists(replacement_path) && fs::is_regular_file(replacement_path)) {
            std::cout << "  发现替换文件: " << info.name << std::endl;

            // 定位到当前封包文件的末尾，准备追加数据
            new_file.seekp(0, std::ios::end);
            uint64_t new_data_offset = new_file.tellp(); // 这就是新数据的偏移

            // 获取替换文件的大小
            uint32_t replacement_size = fs::file_size(replacement_path);

            // 写入8字节的文件头 (不压缩)
            write_u32_le(new_file, 0); // packed_size = 0
            write_u32_le(new_file, replacement_size); // unpacked_size

            // 写入替换文件的内容
            std::ifstream replacement_file(replacement_path, std::ios::binary);
            while (replacement_file) {
                replacement_file.read(copy_buffer.data(), copy_buffer.size());
                new_file.write(copy_buffer.data(), replacement_file.gcount());
            }

            // **核心步骤**: 修改对应索引块的内存副本，更新偏移量
            std::cout << "    -> 追加到封包末尾，新偏移: " << std::hex << new_data_offset << std::endl;
            auto& block_to_patch = index_blocks.at(info.section_offset);
            write_u32_be_to_buffer(block_to_patch, info.entry_pos_in_buffer + 1, static_cast<uint32_t>(new_data_offset));
        }
    }

    std::cout << "所有文件检查完毕，正在将更新后的索引块写回封包..." << std::endl;
    for (const auto& pair : index_blocks) {
        uint32_t section_offset = pair.first;
        const auto& modified_block = pair.second;

        new_file.seekp(section_offset);
        new_file.write(modified_block.data(), modified_block.size());
        std::cout << "  已更新偏移 " << std::hex << section_offset << " 处的索引块。" << std::endl;
    }

    new_file.close();
    std::cout << "\n'追加并修补索引' 模式修改完成！" << std::endl;
}

void print_usage(const char* prog_name) {
    std::cout << "Made by julixian 2025.06.25(这gemini真好用吧)\n";
    std::cout << "Usage(for SHS7 only):\n";
    std::cout << "  Extract: " << prog_name << " extract <.hxp> <output_dir>\n";
    std::cout << "  repack: " << prog_name << " repack <org.hxp> <input_dir> <out.hxp>\n";
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    try {
        if (command == "extract" && argc == 4) {
            extract_archive(argv[2], argv[3]);
        }
        else if (command == "repack" && argc == 5) {
            modify_append_archive(argv[2], argv[3], argv[4]);
        }
        else {
            print_usage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cout << "\n发生错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
