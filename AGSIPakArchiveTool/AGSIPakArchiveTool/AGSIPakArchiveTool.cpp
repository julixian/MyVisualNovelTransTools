#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/des.h>
#include <openssl/err.h>
#include <cstring>

import std;
namespace fs = std::filesystem;

extern "C" size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);
extern "C" size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);

// --- 共享辅助函数和类 ---

template<typename T>
T read_le(const std::vector<uint8_t>& buffer, size_t offset) {
    T value = 0;
    if (offset + sizeof(T) > buffer.size()) {
        throw std::out_of_range("Read out of bounds");
    }
    std::memcpy(&value, buffer.data() + offset, sizeof(T));
    return value;
}

template<typename T>
void write_le(std::ostream& stream, T value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T>
void write_le_vec(std::vector<uint8_t>& vec, size_t offset, T value) {
    if (offset + sizeof(T) > vec.size()) {
        throw std::out_of_range("Write out of bounds in vector");
    }
    std::memcpy(vec.data() + offset, &value, sizeof(T));
}

uint8_t rot_byte_l(uint8_t value, int shift) {
    shift &= 7;
    if (shift == 0) return value;
    return (value << shift) | (value >> (8 - shift));
}

uint8_t rot_byte_r(uint8_t value, int shift) {
    shift &= 7;
    if (shift == 0) return value;
    return (value >> shift) | (value << (8 - shift));
}

class AgsiMersenneTwister {
private:
    static constexpr int StateLength = 624;
    static constexpr int StateM = 397;
    static constexpr uint32_t MatrixA = 0x9908B0DF;
    static constexpr uint32_t SignMask = 0x80000000;
    static constexpr uint32_t LowerMask = 0x7FFFFFFF;
    static constexpr uint32_t TemperingMaskB = 0x9D2C5680;
    static constexpr uint32_t TemperingMaskC = 0xEFC60000;

    std::array<uint32_t, StateLength> mt;
    int mti = StateLength;
    const std::array<uint32_t, 2> mag01 = { 0, MatrixA };

public:
    AgsiMersenneTwister(uint32_t seed) {
        srand(seed);
    }

    void srand(uint32_t seed) {
        for (mti = 0; mti < StateLength; ++mti) {
            uint32_t upper = seed & 0xFFFF0000;
            seed = 69069 * seed + 1;
            mt[mti] = upper | ((seed & 0xFFFF0000) >> 16);
            seed = 69069 * seed + 1;
        }
    }

    uint32_t rand() {
        uint32_t y;
        if (mti >= StateLength) {
            int kk;
            for (kk = 0; kk < StateLength - StateM; kk++) {
                y = (mt[kk] & SignMask) | (mt[kk + 1] & LowerMask);
                mt[kk] = mt[kk + StateM] ^ (y >> 1) ^ mag01[y & 1];
            }
            for (; kk < StateLength - 1; kk++) {
                y = (mt[kk] & SignMask) | (mt[kk + 1] & LowerMask);
                mt[kk] = mt[kk + (StateM - StateLength)] ^ (y >> 1) ^ mag01[y & 1];
            }
            y = (mt[StateLength - 1] & SignMask) | (mt[0] & LowerMask);
            mt[StateLength - 1] = mt[StateM - 1] ^ (y >> 1) ^ mag01[y & 1];
            mti = 0;
        }
        y = mt[mti++];
        y ^= y >> 11;
        y ^= (y << 7) & TemperingMaskB;
        y ^= (y << 15) & TemperingMaskC;
        y ^= y >> 18;
        return y;
    }
};

// --- 数据结构 ---

// 解包用
struct AgsiEntry {
    uint32_t unpacked_size;
    uint32_t size;
    int32_t method;
    uint32_t offset;
    std::string name;
    bool is_encrypted;
    bool is_special;
    bool is_packed;
};

// 封包用
struct PackEntry {
    fs::path source_path;
    std::string name;
    uint32_t size;
    uint32_t offset;
};

void decrypt_header(std::vector<uint8_t>& header, uint8_t k1, uint8_t k2) {
    int shift = k2 & 7;
    if (shift == 0) shift = 1;
    for (size_t i = 0; i < header.size(); ++i) {
        uint8_t x = rot_byte_l(header[i], shift);
        header[i] = x ^ k1++;
    }
}

void decrypt_index(std::vector<uint8_t>& index_data, uint32_t seed) {
    AgsiMersenneTwister rnd(seed);
    for (size_t i = 0; i < index_data.size(); ++i) {
        uint32_t key = rnd.rand();
        int shift = key & 7;
        if (shift == 0) shift = 1;
        uint8_t x = rot_byte_l(index_data[i], shift);
        index_data[i] = static_cast<uint8_t>(key) ^ x;
    }
}

class BitStreamReader {
private:
    const std::vector<uint8_t>& m_buffer;
    size_t m_byte_pos = 0;
    uint8_t m_bit_pos = 0;
public:
    BitStreamReader(const std::vector<uint8_t>& buffer) : m_buffer(buffer) {}
    int get_bit() {
        if (m_byte_pos >= m_buffer.size()) return -1;
        int bit = (m_buffer[m_byte_pos] >> (7 - m_bit_pos)) & 1;
        if (++m_bit_pos == 8) { m_bit_pos = 0; m_byte_pos++; }
        return bit;
    }
    int get_bits(int count) {
        int value = 0;
        for (int i = 0; i < count; ++i) {
            int bit = get_bit();
            if (bit == -1) return -1;
            value = (value << 1) | bit;
        }
        return value;
    }
};

std::vector<uint8_t> decompress_lzbitstream(const std::vector<uint8_t>& input, uint32_t unpacked_size) {
    std::vector<uint8_t> output;
    output.reserve(unpacked_size);
    std::vector<uint8_t> frame(0x1000, 0);
    int frame_pos = 1;
    BitStreamReader reader(input);
    while (output.size() < unpacked_size) {
        int bit = reader.get_bit();
        if (bit == -1) break;
        if (bit != 0) {
            int v = reader.get_bits(8);
            if (v == -1) break;
            uint8_t byte_v = static_cast<uint8_t>(v);
            output.push_back(byte_v);
            frame[frame_pos++ & 0xFFF] = byte_v;
        }
        else {
            int offset = reader.get_bits(12);
            if (offset == -1) break;
            int count = reader.get_bits(4);
            if (count == -1) break;
            count += 2;
            for (int i = 0; i < count; ++i) {
                uint8_t v = frame[offset++ & 0xFFF];
                output.push_back(v);
                frame[frame_pos++ & 0xFFF] = v;
                if (output.size() >= unpacked_size) break;
            }
        }
    }
    return output;
}

std::vector<uint8_t> decompress_lzss(const std::vector<uint8_t>& input, uint32_t unpacked_size) {
    std::vector<uint8_t> output(unpacked_size);
    size_t output_size = lzss_decompress(output.data(), unpacked_size, (uint8_t*)input.data(), input.size());
    if (output_size != unpacked_size) {
        throw std::runtime_error("LZSS output size is not expected");
    }
    return output;
}

void encrypt_index(std::vector<uint8_t>& index_data, uint32_t seed) {
    AgsiMersenneTwister rnd(seed);
    for (size_t i = 0; i < index_data.size(); ++i) {
        uint8_t original_byte = index_data[i];
        uint32_t key = rnd.rand();
        int shift = key & 7;
        if (shift == 0) shift = 1;
        uint8_t key_byte = static_cast<uint8_t>(key);
        index_data[i] = rot_byte_r(original_byte ^ key_byte, shift);
    }
}

void encrypt_header(std::vector<uint8_t>& header, uint8_t k1, uint8_t k2) {
    int shift = k2 & 7;
    if (shift == 0) shift = 1;
    for (size_t i = 0; i < header.size(); ++i) {
        uint8_t original_byte = header[i];
        header[i] = rot_byte_r(original_byte ^ k1++, shift);
    }
}

int do_unpack(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Usage: " << argv[0] << " extract <pak_file> <output_directory> [des_key_hex]" << std::endl;
        return 1;
    }
    fs::path pak_path = argv[2];
    fs::path out_dir = argv[3];
    std::vector<uint8_t> des_key_bytes;
    if (argc == 5) {
        try {
            std::string key_str = argv[4];
            if (key_str.rfind("0x", 0) == 0) key_str = key_str.substr(2);
            if (key_str.length() != 16) throw std::invalid_argument("Key must be 16 hex characters (8 bytes)");
            uint64_t key_val = std::stoull(key_str, nullptr, 16);
            des_key_bytes.resize(8);
            for (int i = 0; i < 8; ++i) des_key_bytes[i] = (key_val >> (56 - i * 8)) & 0xFF;
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing DES key: " << e.what() << std::endl; return 1;
        }
    }
    std::cout << "Opening archive: " << pak_path << std::endl;
    std::ifstream file(pak_path, std::ios::binary | std::ios::ate);
    if (!file) { std::cerr << "Error: Cannot open file " << pak_path << std::endl; return 1; }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> file_data(size);
    if (!file.read(reinterpret_cast<char*>(file_data.data()), size)) { std::cerr << "Error: Cannot read file " << pak_path << std::endl; return 1; }
    std::vector<uint8_t> header(file_data.begin(), file_data.begin() + 12);
    bool is_index_encrypted = false;
    if (std::string(header.begin(), header.begin() + 4) != "PACK") {
        std::cout << "Encrypted header detected. Attempting decryption..." << std::endl;
        if (size < 12) { std::cerr << "Error: File too small for encrypted header." << std::endl; return 1; }
        uint8_t k1 = file_data[size - 9];
        uint8_t k2 = file_data[size - 6];
        decrypt_header(header, k1, k2);
        if (std::string(header.begin(), header.begin() + 4) != "PACK") { std::cerr << "Error: Header decryption failed. Invalid keys or file format." << std::endl; return 1; }
        is_index_encrypted = true;
        std::cout << "Header decrypted successfully." << std::endl;
    }
    int32_t count = read_le<int32_t>(header, 4);
    int32_t record_size = read_le<int32_t>(header, 8);
    uint32_t data_offset = 12 + count * record_size;
    if (count <= 0 || record_size <= 0x10 || data_offset >= size) {
        std::cerr << "Error: Invalid archive header values (count=" << count << ", record_size=" << record_size << ")." << std::endl; return 1;
    }
    std::cout << "Archive contains " << count << " files." << std::endl;
    std::vector<uint8_t> index_data(file_data.begin() + 12, file_data.begin() + data_offset);
    if (is_index_encrypted) {
        std::cout << "Decrypting index..." << std::endl;
        decrypt_index(index_data, 7524u);
        std::cout << "Index decrypted." << std::endl;
    }
    std::vector<AgsiEntry> dir;
    int name_size = record_size - 0x10;
    for (int i = 0; i < count; ++i) {
        size_t record_offset = i * record_size;
        AgsiEntry entry;
        try {
            entry.unpacked_size = read_le<uint32_t>(index_data, record_offset);
            entry.size = read_le<uint32_t>(index_data, record_offset + 4);
            entry.method = read_le<int32_t>(index_data, record_offset + 8);
            entry.offset = read_le<uint32_t>(index_data, record_offset + 12) + data_offset;
            const char* name_ptr = reinterpret_cast<const char*>(index_data.data() + record_offset + 16);
            entry.name = std::string(name_ptr, strnlen(name_ptr, name_size));
        }
        catch (const std::out_of_range&) {
            std::cerr << "Error: Corrupted index record for file #" << i << std::endl; return 1;
        }
        if (entry.name.empty() || (entry.offset + entry.size) > file_data.size()) {
            std::cerr << "Error: Invalid entry #" << i << " (name: '" << entry.name << "', offset/size out of bounds)." << std::endl; return 1;
        }
        entry.is_encrypted = (entry.method >= 3 && entry.method <= 5) || entry.method == 7;
        entry.is_packed = entry.method != 0 && entry.method != 3;
        std::string lower_name = entry.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c) { return std::tolower(c); });
        entry.is_special = (lower_name == "copyright.dat");
        dir.push_back(entry);
    }
    try {
        fs::create_directories(out_dir);
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating output directory: " << e.what() << std::endl; return 1;
    }
    std::cout << "Extracting files to: " << out_dir << std::endl;
    for (const auto& entry : dir) {
        std::cout << " - Extracting " << std::left << std::setw(20) << entry.name << " (Method: " << entry.method << ", Size: " << entry.size << ")" << std::endl;
        std::vector<uint8_t> entry_data(file_data.begin() + entry.offset, file_data.begin() + entry.offset + entry.size);
        if (entry.is_encrypted) {
            if (des_key_bytes.empty()) { std::cerr << "   Warning: Skipping encrypted file. No DES key provided." << std::endl; continue; }
            uint32_t enc_size = entry.size;
            if (enc_size > 1024) enc_size = 1032;
            enc_size &= ~7;
            if (enc_size == 0 && entry.size > 0) { std::cerr << "   Warning: Encrypted part is too small. Skipping." << std::endl; continue; }
            std::vector<uint8_t> encrypted_part(entry_data.begin(), entry_data.begin() + enc_size);
            std::vector<uint8_t> decrypted_part(enc_size);
            DES_cblock key_block;
            std::memcpy(key_block, des_key_bytes.data(), 8);
            DES_key_schedule schedule;
            DES_set_key_unchecked(&key_block, &schedule);
            for (size_t i = 0; i < enc_size; i += 8) DES_ecb_encrypt(reinterpret_cast<const_DES_cblock*>(encrypted_part.data() + i), reinterpret_cast<DES_cblock*>(decrypted_part.data() + i), &schedule, DES_DECRYPT);
            uint32_t header_size;
            if (!entry.is_special) {
                header_size = read_le<uint32_t>(decrypted_part, decrypted_part.size() - 4);
                if (header_size > entry.unpacked_size) { std::cerr << "   Warning: Invalid encryption scheme or key for " << entry.name << ". Skipping." << std::endl; continue; }
            }
            else { header_size = entry.unpacked_size; }
            if (header_size > decrypted_part.size()) { std::cerr << "   Warning: Decrypted header size is larger than decrypted block for " << entry.name << ". Skipping." << std::endl; continue; }
            if (entry.size > enc_size) {
                entry_data.assign(decrypted_part.begin(), decrypted_part.begin() + header_size);
                entry_data.insert(entry_data.end(), file_data.begin() + entry.offset + enc_size, file_data.begin() + entry.offset + entry.size);
            }
            else { entry_data.assign(decrypted_part.begin(), decrypted_part.begin() + header_size); }
        }
        std::vector<uint8_t> final_data;
        bool success = true;
        switch (entry.method) {
        case 0: case 3: final_data = std::move(entry_data); break;
        case 1: case 4: std::cerr << "   Warning: RLE decompression (Method " << entry.method << ") is not implemented. Skipping." << std::endl; success = false; break;
        case 2: case 5: try { final_data = decompress_lzbitstream(entry_data, entry.unpacked_size); }
              catch (const std::exception& e) { std::cerr << "   Error during LZSS decompression: " << e.what() << std::endl; success = false; } break;
        case 6: case 7: try { final_data = decompress_lzss(entry_data, entry.unpacked_size); }
              catch (const std::exception& e) { std::cerr << "   Error during LZSS decompression: " << e.what() << std::endl; success = false; } break;
        default: std::cerr << "   Warning: Unknown method " << entry.method << ". Skipping." << std::endl; success = false; break;
        }
        if (!success) continue;
        fs::path out_path = out_dir / entry.name;
        std::ofstream out_file(out_path, std::ios::binary);
        if (!out_file) { std::cerr << "   Error: Cannot create output file " << out_path << std::endl; continue; }
        out_file.write(reinterpret_cast<const char*>(final_data.data()), final_data.size());
    }
    std::cout << "\nExtraction complete." << std::endl;
    return 0;
}

int do_pack(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Usage: " << argv[0] << " pack <input_directory> <output_pak_file> [--encrypt | -e]" << std::endl;
        return 1;
    }
    fs::path in_dir = argv[2];
    fs::path pak_path = argv[3];
    bool encrypt = false;
    if (argc == 5 && (std::string(argv[4]) == "--encrypt" || std::string(argv[4]) == "-e")) {
        encrypt = true;
    }
    if (!fs::is_directory(in_dir)) {
        std::cerr << "Error: Input path '" << in_dir.string() << "' is not a valid directory." << std::endl;
        return 1;
    }
    std::cout << "Scanning directory: " << in_dir.string() << std::endl;
    std::vector<PackEntry> entries;
    size_t max_filename_len = 0;
    uint32_t current_data_offset = 0;
    for (const auto& dir_entry : fs::directory_iterator(in_dir)) {
        if (dir_entry.is_regular_file()) {
            PackEntry entry;
            entry.source_path = dir_entry.path();
            entry.name = entry.source_path.filename().string();
            if (entry.name.length() > 255) { std::cerr << "Warning: Filename '" << entry.name << "' is too long. Skipping." << std::endl; continue; }
            entry.size = static_cast<uint32_t>(fs::file_size(entry.source_path));
            entry.offset = current_data_offset;
            current_data_offset += entry.size;
            if (entry.name.length() > max_filename_len) max_filename_len = entry.name.length();
            entries.push_back(entry);
        }
    }
    if (entries.empty()) { std::cout << "No files found in the directory. Exiting." << std::endl; return 0; }
    std::cout << "Found " << entries.size() << " files to pack." << std::endl;
    const int32_t base_record_size = 16;
    int32_t record_size = base_record_size + max_filename_len + 1;
    record_size = (record_size + 3) & ~3;
    std::cout << "Calculated index record size: " << record_size << " bytes." << std::endl;
    std::vector<uint8_t> header_data(12);
    std::vector<uint8_t> index_data(entries.size() * record_size);
    std::memcpy(header_data.data(), "PACK", 4);
    write_le_vec(header_data, 4, static_cast<int32_t>(entries.size()));
    write_le_vec(header_data, 8, record_size);
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        size_t record_offset = i * record_size;
        write_le_vec(index_data, record_offset, entry.size);
        write_le_vec(index_data, record_offset + 4, entry.size);
        write_le_vec(index_data, record_offset + 8, 0);
        write_le_vec(index_data, record_offset + 12, entry.offset);
        std::memcpy(index_data.data() + record_offset + 16, entry.name.c_str(), entry.name.length() + 1);
    }
    std::vector<uint8_t> footer(10);
    if (encrypt) {
        std::cout << "Encryption enabled. Encrypting header and index..." << std::endl;
        encrypt_index(index_data, 7524u);
        encrypt_header(header_data, 0, 0);
        for (size_t i = 0; i < footer.size(); ++i) footer[i] = 0;
        std::cout << "Keys generated and data encrypted." << std::endl;
    }
    std::ofstream out_file(pak_path, std::ios::binary);
    if (!out_file) { std::cerr << "Error: Cannot create output file " << pak_path << std::endl; return 1; }
    std::cout << "Writing header and index..." << std::endl;
    out_file.write(reinterpret_cast<const char*>(header_data.data()), header_data.size());
    out_file.write(reinterpret_cast<const char*>(index_data.data()), index_data.size());
    std::cout << "Writing file data..." << std::endl;
    for (const auto& entry : entries) {
        std::cout << " - Packing " << std::left << std::setw(25) << entry.name << " (Size: " << entry.size << ")" << std::endl;
        std::ifstream in_entry_file(entry.source_path, std::ios::binary);
        if (!in_entry_file) {
            std::cerr << "   Error: Could not open source file " << entry.source_path << ". Aborting." << std::endl;
            out_file.close();
            fs::remove(pak_path);
            return 1;
        }
        out_file << in_entry_file.rdbuf();
    }
    if (encrypt) {
        std::cout << "Writing encryption footer..." << std::endl;
        out_file.write(reinterpret_cast<const char*>(footer.data()), footer.size());
    }
    out_file.close();
    std::cout << "\nSuccessfully created archive: " << pak_path << std::endl;
    return 0;
}

void print_usage(fs::path prog_name) {
    std::cout << "Made by julixian 2025.08.19" << std::endl;
    std::cout << "Usage: \n"
        << "For extract: " << prog_name.filename() << " extract <input_pak> <output_dir> [des_key_hex]\n"
        << "For pack: " << prog_name.filename() << " pack <input_dir> <output_pak> [--encrypt | -e]\n"
        << "des_key_hex: should be a 16-character hex string.\n"
        << "--encrypt | -e: encrypt the index and header of the pak file when packing.\n"
        << "For example: \n"
        << prog_name.filename() << " extract data04.pak data04_extracted 0xcf364e455852d3b2\n"
        << prog_name.filename() << " pack data04_repack data04_new.pak -e" 
        << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    if (command == "pack") {
        return do_pack(argc, argv);
    }
    else if (command == "extract") {
        return do_unpack(argc, argv);
    }
    else {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;

}