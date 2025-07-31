#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <numeric>
#include <filesystem>
#include <stdexcept>
#include <random>

inline uint32_t rotL(uint32_t value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

void decrypt(std::vector<uint8_t>& data, uint32_t seed) {
    size_t length = data.size() / 4;
    if (length == 0) {
        return;
    }

    std::vector<uint16_t> ctl(32);
    std::vector<uint32_t> key(32);

    for (int i = 0; i < 32; ++i) {
        uint32_t code = 0;
        uint32_t k = seed;
        for (int j = 0; j < 16; ++j) {
            code = ((k ^ (k >> 1)) << 15) | ((code & 0xFFFF) >> 1);
            k >>= 2;
        }
        key[i] = seed;
        ctl[i] = static_cast<uint16_t>(code);
        seed = rotL(seed, 1);
    }

    uint32_t* data32 = reinterpret_cast<uint32_t*>(data.data());
    for (size_t i = 0; i < length; ++i) {
        uint32_t s = *data32;
        uint16_t code = ctl[i & 0x1F];
        uint32_t d = 0;
        uint32_t v3 = 3;
        uint32_t v2 = 2;
        uint32_t v1 = 1;
        for (int j = 0; j < 16; ++j) {
            if (0 != (code & 1)) {
                d |= (s & v1) << 1 | (s >> 1) & (v2 >> 1);
            }
            else {
                d |= s & v3;
            }
            code >>= 1;
            v3 <<= 2;
            v2 <<= 2;
            v1 <<= 2;
        }
        *data32++ = d ^ key[i & 0x1F];
    }
}

void encrypt(std::vector<uint8_t>& data, uint32_t seed) {
    size_t length = data.size() / 4;
    if (length == 0) return;

    size_t remainder = data.size() % 4;
    if (remainder != 0) {
        data.insert(data.end(), 4 - remainder, 0);
        length = data.size() / 4;
    }

    std::vector<uint16_t> ctl(32);
    std::vector<uint32_t> key(32);

    for (int i = 0; i < 32; ++i) {
        uint32_t code = 0;
        uint32_t k = seed;
        for (int j = 0; j < 16; ++j) {
            code = ((k ^ (k >> 1)) << 15) | ((code & 0xFFFF) >> 1);
            k >>= 2;
        }
        key[i] = seed;
        ctl[i] = static_cast<uint16_t>(code);
        seed = rotL(seed, 1);
    }

    uint32_t* data32 = reinterpret_cast<uint32_t*>(data.data());
    for (size_t i = 0; i < length; ++i) {
        uint32_t plain_text = *data32;
        uint32_t s = plain_text ^ key[i & 0x1F]; // 先异或

        uint16_t code = ctl[i & 0x1F];
        uint32_t d = 0;
        uint32_t v3 = 3;
        uint32_t v2 = 2;
        uint32_t v1 = 1;
        for (int j = 0; j < 16; ++j) {
            if (0 != (code & 1)) {
                d |= (s & v1) << 1 | (s >> 1) & (v2 >> 1);
            }
            else {
                d |= s & v3;
            }
            code >>= 1;
            v3 <<= 2;
            v2 <<= 2;
            v1 <<= 2;
        }
        *data32++ = d; // 写入位重排后的结果
    }
}


namespace Extractor {

    struct PkEntry {
        std::string name;
        int64_t offset;
        uint64_t size;
        int32_t name_offset;
    };

    class MemoryReader {
    private:
        const std::vector<uint8_t>& m_buffer;
        size_t m_pos = 0;
    public:
        MemoryReader(const std::vector<uint8_t>& buffer) : m_buffer(buffer) {}
        template<typename T>
        T read() {
            if (m_pos + sizeof(T) > m_buffer.size()) {
                throw std::runtime_error("Read out of bounds in memory buffer.");
            }
            T value;
            std::memcpy(&value, m_buffer.data() + m_pos, sizeof(T));
            m_pos += sizeof(T);
            return value;
        }
    };

    void extract_archive(const std::filesystem::path& archive_path, const std::filesystem::path& output_dir) {
        std::ifstream file(archive_path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Cannot open file: " + archive_path.string());

        int64_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        char signature[4];
        file.read(signature, 4);
        if (std::string(signature, 4) != "fPK " && std::string(signature, 4) != "fPK2") {
            throw std::runtime_error("Invalid file signature.");
        }
        int version = (signature[3] == '2') ? 2 : 1;
        std::cout << "Detected PK version: " << version << std::endl;

        int64_t base_offset = 0;
        std::vector<PkEntry> dir;
        std::vector<uint8_t> names_buffer;

        auto read_long = [&](std::istream& s) -> int64_t {
            if (version == 2) {
                int64_t val;
                s.read(reinterpret_cast<char*>(&val), sizeof(val));
                return val;
            }
            else {
                uint32_t val;
                s.read(reinterpret_cast<char*>(&val), sizeof(val));
                return val;
            }
            };

        file.seekg(4, std::ios::beg);
        if (file_size != read_long(file)) throw std::runtime_error("File size mismatch in header.");

        while (file.tellg() < file_size && file.good()) {
            int64_t section_start = file.tellg();
            char id_bytes[4];
            file.read(id_bytes, 4);
            if (file.gcount() < 4) break;

            std::string id(id_bytes, 4);
            int64_t section_size = read_long(file);
            int64_t header_size = read_long(file);

            if (section_size < 4 || header_size > section_size) throw std::runtime_error("Invalid section header.");
            int64_t content_pos = section_start + header_size;
            int64_t content_size = section_size - header_size;

            if (id == "cLST") {
                std::cout << "Found cLST (file list) section..." << std::endl;
                file.seekg(4, std::ios::cur); // Skip unknown
                int32_t count;
                file.read(reinterpret_cast<char*>(&count), 4);
                if (count <= 0 || count > 100000) throw std::runtime_error("Invalid or unreasonable file count: " + std::to_string(count));
                uint32_t key;
                file.read(reinterpret_cast<char*>(&key), 4);

                file.seekg(content_pos, std::ios::beg);
                std::vector<uint8_t> clst_buffer(content_size);
                file.read(reinterpret_cast<char*>(clst_buffer.data()), content_size);

                decrypt(clst_buffer, key);

                dir.reserve(count);
                MemoryReader index_reader(clst_buffer);
                auto read_index_long = [&]() -> int64_t {
                    if (version == 2) return index_reader.read<int64_t>();
                    else return index_reader.read<uint32_t>();
                    };
                for (int i = 0; i < count; ++i) {
                    PkEntry entry;
                    entry.name_offset = static_cast<int32_t>(read_index_long());
                    entry.offset = read_index_long();
                    entry.size = static_cast<uint64_t>(read_index_long());
                    dir.push_back(entry);
                }
            }
            else if (id == "cNAM") {
                std::cout << "Found cNAM (file names) section..." << std::endl;
                if (dir.empty()) throw std::runtime_error("cNAM section found before cLST.");
                file.seekg(4, std::ios::cur); // Skip unknown
                uint32_t key;
                file.read(reinterpret_cast<char*>(&key), 4);

                file.seekg(content_pos, std::ios::beg);
                names_buffer.resize(content_size);
                file.read(reinterpret_cast<char*>(names_buffer.data()), content_size);
                decrypt(names_buffer, key);
            }
            else if (id == "cDAT") {
                std::cout << "Found cDAT (file data) section..." << std::endl;
                base_offset = content_pos;
            }
            file.seekg(section_start + section_size, std::ios::beg);
        }

        if (dir.empty() || names_buffer.empty() || base_offset == 0) {
            throw std::runtime_error("Archive is missing required sections (cLST, cNAM, or cDAT).");
        }

        std::cout << "\nStarting extraction to: " << output_dir.string() << std::endl;
        std::filesystem::create_directories(output_dir);
        for (auto& entry : dir) {
            entry.offset += base_offset;
            if (entry.name_offset < 0 || entry.name_offset >= names_buffer.size()) {
                std::cerr << "Warning: Invalid name offset for an entry. Skipping." << std::endl;
                continue;
            }
            entry.name = std::string(reinterpret_cast<const char*>(names_buffer.data() + entry.name_offset));
            if (entry.name.empty()) {
                std::cerr << "Warning: Found an entry with an empty name. Skipping." << std::endl;
                continue;
            }
            std::cout << "Extracting: " << entry.name << " (" << entry.size << " bytes)" << std::endl;
            std::filesystem::path out_path = output_dir / entry.name;
            if (out_path.has_parent_path()) std::filesystem::create_directories(out_path.parent_path());
            std::ofstream out_file(out_path, std::ios::binary);
            if (!out_file) {
                std::cerr << "  -> Failed to create output file. Skipping." << std::endl;
                continue;
            }
            file.seekg(entry.offset, std::ios::beg);
            std::vector<char> buffer(4096);
            uint64_t remaining = entry.size;
            while (remaining > 0) {
                std::streamsize to_read = std::min(static_cast<uint64_t>(buffer.size()), remaining);
                file.read(buffer.data(), to_read);
                std::streamsize read_count = file.gcount();
                if (read_count == 0) {
                    std::cerr << "  -> Unexpected end of archive file. File might be truncated." << std::endl;
                    break;
                }
                out_file.write(buffer.data(), read_count);
                remaining -= read_count;
            }
        }
        std::cout << "\nExtraction complete." << std::endl;
    }
}



namespace Packer {

    struct FileEntry {
        std::filesystem::path disk_path;
        std::string relative_path;
        uint32_t size;
        uint32_t name_offset;
        uint32_t data_offset;
    };

    template<typename T>
    void write_val(std::ostream& os, const T& value) {
        os.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    void pack_archive(const std::filesystem::path& input_dir, const std::filesystem::path& archive_path) {
        if (!std::filesystem::exists(input_dir) || !std::filesystem::is_directory(input_dir)) {
            throw std::runtime_error("Input directory does not exist or is not a directory.");
        }

        std::cout << "Scanning files in " << input_dir.string() << "..." << std::endl;
        std::vector<FileEntry> file_entries;
        uint64_t total_data_size = 0;
        for (const auto& dir_entry : std::filesystem::recursive_directory_iterator(input_dir)) {
            if (dir_entry.is_regular_file()) {
                FileEntry entry;
                entry.disk_path = dir_entry.path();
                entry.relative_path = std::filesystem::relative(dir_entry.path(), input_dir).generic_string();
                entry.size = static_cast<uint32_t>(std::filesystem::file_size(dir_entry.path()));
                file_entries.push_back(entry);
                total_data_size += entry.size;
            }
        }
        if (file_entries.empty()) throw std::runtime_error("Input directory is empty.");
        std::cout << "Found " << file_entries.size() << " files." << std::endl;

        std::cout << "Building cNAM and cLST blocks..." << std::endl;
        std::vector<uint8_t> cnam_buffer;
        uint32_t current_data_offset = 0;
        for (auto& entry : file_entries) {
            entry.name_offset = static_cast<uint32_t>(cnam_buffer.size());
            cnam_buffer.insert(cnam_buffer.end(), entry.relative_path.begin(), entry.relative_path.end());
            cnam_buffer.push_back('\0');
            entry.data_offset = current_data_offset;
            current_data_offset += entry.size;
        }

        std::vector<uint8_t> clst_buffer;
        clst_buffer.reserve(file_entries.size() * 3 * sizeof(uint32_t));
        for (const auto& entry : file_entries) {
            const char* p_name_offset = reinterpret_cast<const char*>(&entry.name_offset);
            const char* p_data_offset = reinterpret_cast<const char*>(&entry.data_offset);
            const char* p_size = reinterpret_cast<const char*>(&entry.size);
            clst_buffer.insert(clst_buffer.end(), p_name_offset, p_name_offset + sizeof(uint32_t));
            clst_buffer.insert(clst_buffer.end(), p_data_offset, p_data_offset + sizeof(uint32_t));
            clst_buffer.insert(clst_buffer.end(), p_size, p_size + sizeof(uint32_t));
        }

        std::cout << "Encrypting metadata..." << std::endl;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> distrib;
        uint32_t clst_key = distrib(gen);
        uint32_t cnam_key = distrib(gen);

        encrypt(clst_buffer, clst_key);
        encrypt(cnam_buffer, cnam_key);

        const uint32_t section_base_header_size = 12;
        const uint32_t clst_header_content_size = 12;
        const uint32_t cnam_header_content_size = 8;
        const uint32_t cdat_header_content_size = 0;
        uint32_t clst_header_size = section_base_header_size + clst_header_content_size;
        uint32_t clst_section_size = clst_header_size + static_cast<uint32_t>(clst_buffer.size());
        uint32_t cnam_header_size = section_base_header_size + cnam_header_content_size;
        uint32_t cnam_section_size = cnam_header_size + static_cast<uint32_t>(cnam_buffer.size());
        uint32_t cdat_header_size = section_base_header_size + cdat_header_content_size;
        uint32_t cdat_section_size = cdat_header_size + static_cast<uint32_t>(total_data_size);
        uint32_t total_file_size = 8 + clst_section_size + cnam_section_size + cdat_section_size;

        std::cout << "Writing archive to " << archive_path.string() << "..." << std::endl;
        std::ofstream out_file(archive_path, std::ios::binary);
        if (!out_file) throw std::runtime_error("Failed to create output file.");

        out_file.write("fPK ", 4);
        write_val(out_file, total_file_size);
        out_file.write("cLST", 4);
        write_val(out_file, clst_section_size);
        write_val(out_file, clst_header_size);
        write_val(out_file, (uint32_t)1);
        write_val(out_file, (int32_t)file_entries.size());
        write_val(out_file, clst_key);
        out_file.write(reinterpret_cast<const char*>(clst_buffer.data()), clst_buffer.size());
        out_file.write("cNAM", 4);
        write_val(out_file, cnam_section_size);
        write_val(out_file, cnam_header_size);
        write_val(out_file, (uint32_t)0);
        write_val(out_file, cnam_key);
        out_file.write(reinterpret_cast<const char*>(cnam_buffer.data()), cnam_buffer.size());
        out_file.write("cDAT", 4);
        write_val(out_file, cdat_section_size);
        write_val(out_file, cdat_header_size);

        std::cout << "Writing file data..." << std::endl;
        std::vector<char> read_buffer(4096);
        for (const auto& entry : file_entries) {
            std::ifstream in_file(entry.disk_path, std::ios::binary);
            while (in_file) {
                in_file.read(read_buffer.data(), read_buffer.size());
                out_file.write(read_buffer.data(), in_file.gcount());
            }
        }
        std::cout << "\nPacking complete." << std::endl;
    }
}


void print_usage(const char* prog_name) {
    std::cout << "Made by julixian 2025.07.31" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  To extract: " << prog_name << " extract <archive.pk> <output_directory>" << std::endl;
    std::cout << "  To pack(only support version 1):    " << prog_name << " pack <input_directory> <output_archive.pk>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    std::filesystem::path path1(argv[2]);
    std::filesystem::path path2(argv[3]);

    try {
        if (command == "extract") {
            Extractor::extract_archive(path1, path2);
        }
        else if (command == "pack") {
            Packer::pack_archive(path1, path2);
        }
        else {
            std::cerr << "Error: Unknown command '" << command << "'" << std::endl << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
