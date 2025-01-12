#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <cstring>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

#pragma pack(1)
struct ExeEntry {
    uint16_t is_compressed;
    uint16_t resource_id;
    uint32_t offset;
    uint32_t length;
};

struct OldExeEntry {
    uint32_t offset;
    uint32_t length;
    uint16_t is_compressed;
    uint16_t resource_id;
};
#pragma pack()

class PacExtractor {
private:
    std::string exe_path;
    std::string pac_path;
    std::string output_dir;
    std::vector<uint8_t> exe_data;
    std::unique_ptr<std::ifstream> pac_file;
    std::ofstream log_file;

    struct ResourceEntry {
        uint32_t resource_id;
        uint32_t offset;
        uint32_t length;
        bool is_compressed;
    };
    std::vector<ResourceEntry> resources;

public:
    PacExtractor(const std::string& exe_path, const std::string& pac_path,
        const std::string& output_dir)
        : exe_path(exe_path), pac_path(pac_path), output_dir(output_dir) {
    }

    bool initialize() {
        // 读取exe文件
        std::ifstream exe_file(exe_path, std::ios::binary);
        if (!exe_file) {
            std::cerr << "Failed to open exe file: " << exe_path << std::endl;
            return false;
        }

        // 获取文件大小
        exe_file.seekg(0, std::ios::end);
        size_t file_size = exe_file.tellg();
        exe_file.seekg(0, std::ios::beg);

        // 读取整个文件
        exe_data.resize(file_size);
        exe_file.read(reinterpret_cast<char*>(exe_data.data()), file_size);
        exe_file.close();

        // 打开PAC文件
        pac_file = std::make_unique<std::ifstream>(pac_path, std::ios::binary);
        if (!pac_file->is_open()) {
            std::cerr << "Failed to open PAC file: " << pac_path << std::endl;
            return false;
        }

        // 创建输出目录
        if (!fs::exists(output_dir)) {
            fs::create_directories(output_dir);
        }

        // 打开日志文件
        log_file.open(output_dir + "/extract.log");
        if (!log_file) {
            std::cerr << "Failed to create log file" << std::endl;
            return false;
        }

        writeHeader();
        return true;
    }

    bool extractResources() {
        // 搜索并解析索引
        if (!parseIndex()) {
            return false;
        }

        // 提取每个资源
        for (const auto& res : resources) {
            if (!extractResource(res)) {
                std::cerr << "Failed to extract resource " << res.resource_id << std::endl;
                continue;
            }
        }

        writeStatistics();
        return true;
    }

private:
    void writeHeader() {
        log_file << "PAC Extraction Report" << std::endl;
        log_file << "EXE File: " << exe_path << std::endl;
        log_file << "PAC File: " << pac_path << std::endl;
        log_file << "Output Directory: " << output_dir << std::endl;
        log_file << std::string(80, '-') << std::endl;
    }

    bool parseIndex() {
        // 先尝试新格式
        auto index_pos = searchIndex(false);
        if (index_pos != nullptr) {
            log_file << "Using new format index table" << std::endl;
            return parseNewFormatIndex(index_pos);
        }

        // 再尝试旧格式
        index_pos = searchIndex(true);
        if (index_pos != nullptr) {
            log_file << "Using old format index table" << std::endl;
            return parseOldFormatIndex(index_pos);
        }

        log_file << "No valid index table found" << std::endl;
        return false;
    }

    uint8_t* searchIndex(bool old_format) {
        size_t entry_size = old_format ? sizeof(OldExeEntry) : sizeof(ExeEntry);
        uint8_t* p = exe_data.data();
        uint8_t* end = p + (exe_data.size() - entry_size * 2);

        while (p < end) {
            if (old_format) {
                auto* entry = reinterpret_cast<OldExeEntry*>(p);
                auto* next_entry = entry + 1;

                if (!entry->offset &&
                    (((entry->length + 255) & ~255) == next_entry->offset) &&
                    (static_cast<int32_t>(entry->length) > 0) &&
                    (!entry->is_compressed || entry->is_compressed == 1) &&
                    (entry->resource_id == 1)) {
                    return p;
                }
            }
            else {
                auto* entry = reinterpret_cast<ExeEntry*>(p);
                auto* next_entry = entry + 1;

                if (!entry->offset &&
                    (((entry->length + 2047) & ~2047) == next_entry->offset) &&
                    (static_cast<int32_t>(entry->length) > 0) &&
                    (!entry->is_compressed || entry->is_compressed == 1) &&
                    (entry->resource_id == 1)) {
                    return p;
                }
            }
            p += 4;
        }

        return nullptr;
    }

    bool parseNewFormatIndex(uint8_t* index_start) {
        auto* entry = reinterpret_cast<ExeEntry*>(index_start);

        while (true) {
            // 检查是否到达索引表末尾
            if (entry->offset && !entry->resource_id &&
                (entry->offset == entry->length)) {
                break;
            }

            ResourceEntry res;
            res.resource_id = entry->resource_id;
            res.offset = entry->offset;
            res.length = entry->length;
            res.is_compressed = entry->is_compressed != 0;

            resources.push_back(res);
            entry++;
        }

        return !resources.empty();
    }

    bool parseOldFormatIndex(uint8_t* index_start) {
        auto* entry = reinterpret_cast<OldExeEntry*>(index_start);

        while (true) {
            if (entry->offset && !entry->resource_id &&
                (entry->offset == entry->length)) {
                break;
            }

            ResourceEntry res;
            res.resource_id = entry->resource_id;
            res.offset = entry->offset;
            res.length = entry->length;
            res.is_compressed = entry->is_compressed != 0;

            resources.push_back(res);
            entry++;
        }

        return !resources.empty();
    }

    bool extractResource(const ResourceEntry& res) {
        // 构建输出文件名
        std::string filename = std::to_string(res.resource_id);
        filename = std::string(5 - filename.length(), '0') + filename;
        std::string output_path = output_dir + "/script" + filename + ".bin";

        // 创建输出文件
        std::ofstream out_file(output_path, std::ios::binary);
        if (!out_file) {
            log_file << "Failed to create output file: " << output_path << std::endl;
            return false;
        }

        // 定位到资源位置
        pac_file->seekg(res.offset, std::ios::beg);
        if (pac_file->fail()) {
            log_file << "Failed to seek to resource offset: " << res.offset << std::endl;
            return false;
        }

        // 分配缓冲区
        std::vector<uint8_t> buffer(res.length);

        // 读取资源数据
        pac_file->read(reinterpret_cast<char*>(buffer.data()), res.length);
        if (pac_file->fail()) {
            log_file << "Failed to read resource data, id: " << res.resource_id << std::endl;
            return false;
        }

        // 写入输出文件
        out_file.write(reinterpret_cast<char*>(buffer.data()), res.length);
        if (out_file.fail()) {
            log_file << "Failed to write resource data, id: " << res.resource_id << std::endl;
            return false;
        }

        // 记录日志
        log_file << "Extracted resource " << filename
            << " (ID: " << res.resource_id
            << ", Offset: 0x" << std::hex << res.offset
            << ", Length: 0x" << res.length
            << ", Compressed: " << (res.is_compressed ? "Yes" : "No")
            << ")" << std::endl;

        return true;
    }

    void writeStatistics() {
        log_file << "\nExtraction Statistics:" << std::endl;
        log_file << "Total Resources: " << resources.size() << std::endl;

        size_t compressed_count = 0;
        uint64_t total_size = 0;

        for (const auto& res : resources) {
            if (res.is_compressed) compressed_count++;
            total_size += res.length;
        }

        log_file << "Compressed Resources: " << compressed_count << std::endl;
        log_file << "Total Size: " << total_size << " bytes" << std::endl;
    }
};

class PacPacker {
private:
    static const uint32_t ALIGNMENT = 0x800;  // 2048字节对齐
    std::vector<ExeEntry> index_entries;
    fs::path exe_path;
    fs::path input_dir;
    fs::path output_dir;
    std::vector<uint8_t> exe_data;
    uint8_t* index_location = nullptr;
    size_t index_entries_count = 0;

public:
    PacPacker(const fs::path& exe, const fs::path& input, const fs::path& output)
        : exe_path(exe), input_dir(input), output_dir(output) {}

    bool execute() {
        if (!loadExe()) return false;
        if (!findIndex()) return false;
        if (!createPac()) return false;
        if (!updateExe()) return false;
        return true;
    }

private:
    bool loadExe() {
        std::ifstream exe_file(exe_path, std::ios::binary);
        if (!exe_file) {
            std::cerr << "Failed to open exe file: " << exe_path << std::endl;
            return false;
        }

        exe_file.seekg(0, std::ios::end);
        size_t file_size = exe_file.tellg();
        exe_file.seekg(0, std::ios::beg);

        exe_data.resize(file_size);
        if (!exe_file.read(reinterpret_cast<char*>(exe_data.data()), file_size)) {
            std::cerr << "Failed to read exe file" << std::endl;
            return false;
        }

        return true;
    }

    bool findIndex() {
        uint8_t* p = exe_data.data();
        uint8_t* end = p + (exe_data.size() - sizeof(ExeEntry) * 2);

        while (p < end) {
            ExeEntry* entry = reinterpret_cast<ExeEntry*>(p);
            ExeEntry* next_entry = entry + 1;

            if (!entry->offset &&
                (((entry->length + 2047) & ~2047) == next_entry->offset) &&
                (static_cast<int32_t>(entry->length) > 0) &&
                (!entry->is_compressed || entry->is_compressed == 1) &&
                (entry->resource_id == 1)) {
                index_location = p;
                // 计算索引表条目数
                uint8_t* index_end = p;
                while (index_end < end) {
                    ExeEntry* curr_entry = reinterpret_cast<ExeEntry*>(index_end);
                    if (curr_entry->offset && !curr_entry->resource_id &&
                        (curr_entry->offset == curr_entry->length)) {
                        break;
                    }
                    index_entries.push_back(*curr_entry);
                    index_end += sizeof(ExeEntry);
                }
                index_entries_count = index_entries.size();
                return true;
            }
            p += 4;
        }

        std::cerr << "Failed to find index in exe" << std::endl;
        return false;
    }

    bool createPac() {
        fs::path pac_path = output_dir / "script.pac";
        std::ofstream pac_file(pac_path, std::ios::binary);
        if (!pac_file) {
            std::cerr << "Failed to create PAC file" << std::endl;
            return false;
        }

        uint32_t current_offset = 0;
        std::vector<uint8_t> padding(ALIGNMENT, 0);

        // 处理每个文件
        for (size_t i = 0; i < index_entries_count; ++i) {
            // 构造输入文件名
            std::stringstream ss;
            ss << "script" << std::setfill('0') << std::setw(5) << (i + 1) << ".bin";
            fs::path input_file = input_dir / ss.str();

            // 读取输入文件
            std::ifstream input(input_file, std::ios::binary);
            if (!input) {
                std::cerr << "Failed to open input file: " << input_file << std::endl;
                return false;
            }

            // 获取文件大小
            input.seekg(0, std::ios::end);
            uint32_t file_size = static_cast<uint32_t>(input.tellg());
            input.seekg(0, std::ios::beg);

            // 读取文件数据
            std::vector<uint8_t> file_data(file_size);
            input.read(reinterpret_cast<char*>(file_data.data()), file_size);

            // 更新索引信息
            index_entries[i].offset = current_offset;
            index_entries[i].length = file_size;
            index_entries[i].is_compressed = 0;  // 设置为不压缩

            // 写入文件数据
            pac_file.write(reinterpret_cast<char*>(file_data.data()), file_size);

            // 计算需要的填充大小
            uint32_t aligned_offset = (current_offset + file_size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
            uint32_t padding_size = aligned_offset - (current_offset + file_size);

            // 写入填充数据
            if (padding_size > 0) {
                pac_file.write(reinterpret_cast<char*>(padding.data()), padding_size);
            }

            // 更新偏移
            current_offset = aligned_offset;

            std::cout << "Packed file " << input_file.filename()
                << " at offset 0x" << std::hex << index_entries[i].offset
                << " (size: 0x" << file_size << ")" << std::endl;
        }

        pac_file.close();
        return true;
    }

    bool updateExe() {
        fs::path new_exe_path = output_dir / exe_path.filename();

        // 更新exe中的索引数据
        if (index_location) {
            std::memcpy(index_location, index_entries.data(),
                index_entries.size() * sizeof(ExeEntry));
        }

        // 写入新的exe文件
        std::ofstream new_exe(new_exe_path, std::ios::binary);
        if (!new_exe) {
            std::cerr << "Failed to create new exe file" << std::endl;
            return false;
        }

        new_exe.write(reinterpret_cast<char*>(exe_data.data()), exe_data.size());
        new_exe.close();

        std::cout << "\nCreated new exe file: " << new_exe_path << std::endl;
        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cout << "Made by julixian 2025.01.12" << std::endl;
        std::cout << "Usage: " << argv[0] << " <mode> <exe_path> <pac_path/input_dir> <release_dir>\n"
            << "Modes:\n"
            << "  -u    Unpack script.pac\n"
            << "  -p    Pack files into script.pac\n"
            << "Examples:\n"
            << "  Unpack: " << argv[0] << " -u game.exe script.pac output_dir\n"
            << "  Pack:   " << argv[0] << " -p game.exe input_dir output_dir\n";
        return 1;
    }

    std::string mode = argv[1];
    fs::path exe_path = argv[2];
    fs::path input_path = argv[3];
    fs::path output_dir = argv[4];

    if (mode == "-u") {
        // 解包模式
        PacExtractor extractor(exe_path.string(), input_path.string(), output_dir.string());
        if (!extractor.initialize()) {
            std::cerr << "Failed to initialize extractor" << std::endl;
            return 1;
        }

        if (!extractor.extractResources()) {
            std::cerr << "Failed to extract resources" << std::endl;
            return 1;
        }

        std::cout << "Resource extraction completed successfully" << std::endl;
    }
    else if (mode == "-p") {
        // 封包模式
        PacPacker packer(exe_path, input_path, output_dir);
        if (!packer.execute()) {
            std::cerr << "Packing failed" << std::endl;
            return 1;
        }

        std::cout << "Packing completed successfully" << std::endl;
    }
    else {
        std::cerr << "Invalid mode. Use -u for unpack or -p for pack" << std::endl;
        return 1;
    }

    return 0;
}
