#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <zlib.h>

#pragma pack(push, 1)
struct pak_header_t {
    char magic[48];
    uint32_t reserved[2];
    uint32_t index_entries;
    uint32_t zero;
};

struct pak_entry_t {
    char name[48];
    uint32_t offset0;
    uint32_t offset1;
    uint32_t length;
    uint32_t zero;
};
#pragma pack(pop)

uint32_t find_data_start(std::ifstream& file, uint32_t start_pos) {
    file.seekg(start_pos);
    std::vector<char> buffer(32);
    uint32_t current_pos = start_pos;

    while (true) {
        file.read(buffer.data(), 32);
        if (file.gcount() != 32) {
            break;
        }

        if (std::any_of(buffer.begin(), buffer.end(), [](char c) { return c != 0; })) {
            return current_pos;
        }

        current_pos += 32;
    }

    return start_pos; // If no non-zero data found, return the original start position
}

bool extract_pak(const std::string& pak_path, const std::string& output_dir, bool check_magic = true) {
    std::ifstream file(pak_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << pak_path << std::endl;
        return false;
    }

    pak_header_t header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (check_magic && strncmp(header.magic, "DATA$TOP", 8) != 0) {
        std::cerr << "Invalid PAK file format" << std::endl;
        return false;
    }

    std::vector<pak_entry_t> entries(header.index_entries - 1);
    file.read(reinterpret_cast<char*>(entries.data()), entries.size() * sizeof(pak_entry_t));

    uint32_t expected_data_start = sizeof(pak_header_t) + entries.size() * sizeof(pak_entry_t);
    uint32_t actual_data_start = find_data_start(file, expected_data_start);

    for (auto& entry : entries) {
        entry.offset1 += actual_data_start;
    }

    std::filesystem::create_directories(output_dir);

    std::string filename_list_path = std::filesystem::path(pak_path).stem().string() + "_filelist.txt";
    std::ofstream filename_list(filename_list_path);
    if (!filename_list) {
        std::cerr << "Failed to create filename list file" << std::endl;
        return false;
    }

    filename_list << "Data start offset: " << actual_data_start << std::endl;

    for (const auto& entry : entries) {
        filename_list << entry.name << std::endl;

        std::string output_path = output_dir + "/" + entry.name;

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(output_path).parent_path(), ec);
        if (ec) {
            std::cerr << "Failed to create directories for: " << output_path << std::endl;
            std::cerr << "Error: " << ec.message() << std::endl;
            continue;
        }

        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            std::cerr << "Failed to create file: " << output_path << std::endl;
            std::cerr << "Error: " << std::strerror(errno) << std::endl;
            continue;
        }

        file.seekg(entry.offset1, std::ios::beg);
        std::vector<char> buffer(entry.length);
        file.read(buffer.data(), entry.length);

        if (file.gcount() != entry.length) {
            std::cerr << "Warning: Read " << file.gcount() << " bytes, expected " << entry.length << " bytes for file: " << entry.name << std::endl;
        }

        output.write(buffer.data(), file.gcount());

        std::cout << "Extracted: " << entry.name << " (" << file.gcount() << " bytes)" << std::endl;
    }

    filename_list.close();
    std::cout << "Filename list created: " << filename_list_path << std::endl;

    return true;
}

bool create_pak(const std::string& list_file, const std::string& input_dir, const std::string& output_pak, bool zero_magic = false) {
    std::ifstream file_list(list_file);
    if (!file_list) {
        std::cerr << "Failed to open list file: " << list_file << std::endl;
        return false;
    }

    uint32_t data_start_offset;
    std::string line;
    std::getline(file_list, line);
    if (sscanf(line.c_str(), "Data start offset: %u", &data_start_offset) != 1) {
        std::cerr << "Failed to read data start offset from list file" << std::endl;
        return false;
    }

    std::vector<std::string> filenames;
    while (std::getline(file_list, line)) {
        filenames.push_back(line);
    }

    pak_header_t header = {};
    if (!zero_magic) {
        std::strncpy(header.magic, "DATA$TOP", 8);
    }
    header.index_entries = filenames.size() + 1;

    std::vector<pak_entry_t> entries(filenames.size());

    uint32_t expected_data_start = sizeof(pak_header_t) + entries.size() * sizeof(pak_entry_t);
    uint32_t padding_size = data_start_offset - expected_data_start;

    uint32_t current_offset = 0;
    uint32_t total_size = data_start_offset;

    for (size_t i = 0; i < filenames.size(); ++i) {
        std::strncpy(entries[i].name, filenames[i].c_str(), sizeof(entries[i].name) - 1);
        entries[i].offset0 = current_offset;
        entries[i].offset1 = current_offset;

        std::string full_path = input_dir + "/" + filenames[i];
        if (std::filesystem::exists(full_path)) {
            entries[i].length = std::filesystem::file_size(full_path);
        }
        else {
            std::cerr << "Warning: File not found: " << full_path << std::endl;
            entries[i].length = 0;
        }

        current_offset += entries[i].length;
        total_size += entries[i].length;
    }

    std::ofstream pak_file(output_pak, std::ios::binary);
    if (!pak_file) {
        std::cerr << "Failed to create output .pak file: " << output_pak << std::endl;
        return false;
    }

    pak_file.write(reinterpret_cast<char*>(&header), sizeof(header));
    pak_file.write(reinterpret_cast<char*>(entries.data()), entries.size() * sizeof(pak_entry_t));

    // Write padding
    std::vector<char> padding(padding_size, 0);
    pak_file.write(padding.data(), padding_size);

    for (const auto& entry : entries) {
        std::string full_path = input_dir + "/" + entry.name;
        std::ifstream input_file(full_path, std::ios::binary);
        if (input_file) {
            pak_file << input_file.rdbuf();
            size_t file_padding = entry.length - input_file.tellg();
            std::vector<char> zeros(file_padding, 0);
            pak_file.write(zeros.data(), file_padding);
        }
        else {
            std::vector<char> zeros(entry.length, 0);
            pak_file.write(zeros.data(), entry.length);
        }
    }

    std::cout << "PAK file created successfully: " << output_pak << std::endl;
    return true;
}

bool extract_z(const std::string& z_path, const std::string& output_dir) {
    std::ifstream file(z_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open file: " << z_path << std::endl;
        return false;
    }

    std::streamsize compressed_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> compressed_data(compressed_size);
    if (!file.read(compressed_data.data(), compressed_size)) {
        std::cerr << "Failed to read compressed data" << std::endl;
        return false;
    }

    uLongf uncompressed_size = compressed_size * 2; // Initial guess
    std::vector<char> uncompressed_data;

    while (true) {
        uncompressed_data.resize(uncompressed_size);
        int result = uncompress(reinterpret_cast<Bytef*>(uncompressed_data.data()), &uncompressed_size,
            reinterpret_cast<const Bytef*>(compressed_data.data()), compressed_size);

        if (result == Z_OK) {
            break;
        }
        else if (result == Z_BUF_ERROR) {
            uncompressed_size *= 2;
        }
        else {
            std::cerr << "Decompression failed" << std::endl;
            return false;
        }
    }

    uncompressed_data.resize(uncompressed_size);

    std::string pak_path = std::filesystem::path(z_path).stem().string() + ".pak";
    std::ofstream pak_file(pak_path, std::ios::binary);
    if (!pak_file) {
        std::cerr << "Failed to create PAK file: " << pak_path << std::endl;
        return false;
    }

    pak_file.write(uncompressed_data.data(), uncompressed_size);
    pak_file.close(); // Explicitly close the file to ensure all data is written

    // Use a new ifstream to read the PAK file
    std::ifstream pak_input(pak_path, std::ios::binary);
    if (!pak_input) {
        std::cerr << "Failed to open created PAK file for reading" << std::endl;
        return false;
    }

    bool result = extract_pak(pak_path, output_dir, false);

    pak_input.close();
    std::filesystem::remove(pak_path); // Remove the temporary PAK file

    return result;
}

bool create_z(const std::string& list_file, const std::string& input_dir, const std::string& output_z) {
    std::string temp_pak = std::filesystem::temp_directory_path().string() + "/temp.pak";
    if (!create_pak(list_file, input_dir, temp_pak, true)) {  // Create PAK with zero magic
        return false;
    }

    std::ifstream pak_file(temp_pak, std::ios::binary | std::ios::ate);
    if (!pak_file) {
        std::cerr << "Failed to open temporary PAK file" << std::endl;
        return false;
    }

    std::streamsize uncompressed_size = pak_file.tellg();
    pak_file.seekg(0, std::ios::beg);

    std::vector<char> uncompressed_data(uncompressed_size);
    if (!pak_file.read(uncompressed_data.data(), uncompressed_size)) {
        std::cerr << "Failed to read uncompressed data" << std::endl;
        return false;
    }

    pak_file.close();
    std::filesystem::remove(temp_pak);

    uLongf compressed_size = compressBound(uncompressed_size);
    std::vector<char> compressed_data(compressed_size);

    int result = compress2(reinterpret_cast<Bytef*>(compressed_data.data()), &compressed_size,
        reinterpret_cast<const Bytef*>(uncompressed_data.data()), uncompressed_size,
        Z_BEST_COMPRESSION);

    if (result != Z_OK) {
        std::cerr << "Compression failed" << std::endl;
        return false;
    }

    compressed_data.resize(compressed_size);

    std::ofstream z_file(output_z, std::ios::binary);
    if (!z_file) {
        std::cerr << "Failed to create Z file: " << output_z << std::endl;
        return false;
    }

    z_file.write(compressed_data.data(), compressed_size);
    std::cout << "Compressed Z file created: " << output_z << std::endl;

    return true;
}

void print_usage(const char* program_name) {
    std::cout << "Made by julixian 2025.01.29" << std::endl;
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  Extract PAK: " << program_name << " extract_pak <pak_file> <output_directory>" << std::endl;
    std::cerr << "  Create PAK:  " << program_name << " create_pak <list_file> <input_directory> <output_pak>" << std::endl;
    std::cerr << "  Extract Z:   " << program_name << " extract_z <z_file> <output_directory>" << std::endl;
    std::cerr << "  Create Z:    " << program_name << " create_z <list_file> <input_directory> <output_z>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "extract_pak" && argc == 4) {
        std::string pak_path = argv[2];
        std::string output_dir = argv[3];
        return extract_pak(pak_path, output_dir) ? 0 : 1;
    }
    else if (command == "create_pak" && argc == 5) {
        std::string list_file = argv[2];
        std::string input_dir = argv[3];
        std::string output_pak = argv[4];
        return create_pak(list_file, input_dir, output_pak) ? 0 : 1;
    }
    else if (command == "extract_z" && argc == 4) {
        std::string z_path = argv[2];
        std::string output_dir = argv[3];
        return extract_z(z_path, output_dir) ? 0 : 1;
    }
    else if (command == "create_z" && argc == 5) {
        std::string list_file = argv[2];
        std::string input_dir = argv[3];
        std::string output_z = argv[4];
        return create_z(list_file, input_dir, output_z) ? 0 : 1;
    }
    else {
        print_usage(argv[0]);
        return 1;
    }
}
