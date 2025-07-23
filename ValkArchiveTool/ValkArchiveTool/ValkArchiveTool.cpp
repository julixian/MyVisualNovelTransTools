#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <Windows.h>

std::string wide2Ascii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
        (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte
        (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst) {
    return wide2Ascii(ascii2Wide(ascii, src), dst);
}

const uint32_t INDEX_ENTRY_SIZE = 0x10C;

struct IndexEntry {
    std::string filename;
    uint32_t encrypted_offset;
    uint32_t encrypted_size;
};

bool read_u32(std::ifstream& ifs, uint32_t& value) {
    return static_cast<bool>(ifs.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Made by julixian 2025.07.23" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <package_file> <output_directory>" << std::endl;
        return 1;
    }

    std::filesystem::path package_path = argv[1];
    std::filesystem::path output_dir = argv[2];

    std::ifstream archive(package_path, std::ios::binary);
    if (!archive) {
        std::cerr << "Error: Cannot open package file '" << package_path << "'" << std::endl;
        return 1;
    }

    try {
        std::filesystem::create_directories(output_dir);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: Cannot create output directory '" << output_dir << "': " << e.what() << std::endl;
        return 1;
    }

    archive.seekg(0x4);
    uint32_t index_area_size;
    if (!read_u32(archive, index_area_size)) {
        std::cerr << "Error: Failed to read index area size." << std::endl;
        return 1;
    }

    if (index_area_size == 0 || index_area_size % INDEX_ENTRY_SIZE != 0) {
        std::cerr << "Error: Invalid index area size: " << index_area_size << std::endl;
        return 1;
    }

    uint32_t num_files = index_area_size / INDEX_ENTRY_SIZE;
    uint32_t data_area_start = index_area_size + 0x8;

    std::cout << "Package Info:" << std::endl;
    std::cout << "  - Index Size: " << index_area_size << " bytes" << std::endl;
    std::cout << "  - File Count: " << num_files << std::endl;
    std::cout << "  - Data Area Start: 0x" << std::hex << data_area_start << std::dec << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    archive.seekg(0x8);
    std::vector<IndexEntry> indices;
    indices.reserve(num_files);

    for (uint32_t i = 0; i < num_files; ++i) {
        char entry_buffer[INDEX_ENTRY_SIZE];
        if (!archive.read(entry_buffer, INDEX_ENTRY_SIZE)) {
            std::cerr << "Error: Failed to read index entry #" << i << std::endl;
            return 1;
        }

        IndexEntry entry;
        entry.filename = std::string(entry_buffer);
        entry.encrypted_offset = *reinterpret_cast<uint32_t*>(entry_buffer + 0x104);
        entry.encrypted_size = *reinterpret_cast<uint32_t*>(entry_buffer + 0x108);

        indices.push_back(entry);
    }

    uint32_t current_data_offset = 0;

    for (size_t i = 0; i < indices.size(); ++i) {
        const auto& entry = indices[i];

        uint32_t key = entry.encrypted_offset ^ current_data_offset;

        uint32_t true_size = entry.encrypted_size ^ key;

        std::cout << "Extracting [" << (i + 1) << "/" << num_files << "]: " << entry.filename
            << " (Size: " << true_size << " bytes)" << std::endl;

        std::filesystem::path output_file_path = output_dir / ascii2Wide(entry.filename, 932);

        if (output_file_path.has_parent_path()) {
            std::filesystem::create_directories(output_file_path.parent_path());
        }

        std::ofstream outfile(output_file_path, std::ios::binary | std::ios::trunc);
        if (!outfile) {
            std::cerr << "  -> Error: Failed to create output file '" << output_file_path << "'" << std::endl;
            current_data_offset += true_size;
            continue;
        }

        archive.seekg(data_area_start + current_data_offset);

        std::vector<char> file_buffer(true_size);
        if (!archive.read(file_buffer.data(), true_size)) {
            std::cerr << "  -> Error: Failed to read " << true_size << " bytes from archive for file " << entry.filename << std::endl;
            current_data_offset += true_size;
            continue;
        }

        outfile.write(file_buffer.data(), true_size);
        outfile.close();

        current_data_offset += true_size;
    }

    archive.close();
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Extraction complete. " << num_files << " files extracted to " << output_dir << std::endl;

    return 0;
}
