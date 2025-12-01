#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <Windows.h>
#include <cstdint>

#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

import std;
namespace fs = std::filesystem;

// --- 加密/解密与辅助函数 ---

template<typename T>
T read(void* ptr)
{
    T value;
    memcpy(&value, ptr, sizeof(T));
    return value;
}

template<typename T>
void write(void* ptr, T value)
{
    memcpy(ptr, &value, sizeof(T));
}

std::string wide2Ascii(const std::wstring& wide, UINT CodePage = CP_UTF8);
std::wstring ascii2Wide(const std::string& ascii, UINT CodePage = CP_ACP);
std::string ascii2Ascii(const std::string& ascii, UINT src = CP_ACP, UINT dst = CP_UTF8);

std::string wide2Ascii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst) {
    return wide2Ascii(ascii2Wide(ascii, src), dst);
}

constexpr DWORD key1 = 0xD17ACA4B;
constexpr int shift1 = 2;
constexpr DWORD key2 = 0x6269B97C;
constexpr int shift2 = 3;
constexpr DWORD key3 = 0x915C1A4C;
constexpr int shift3 = 5;
constexpr DWORD key4 = 0xBD3ACCDD;
constexpr int shift4 = 3;

DWORD decyptMode1(DWORD eDword, DWORD indexDataSize) {
    return indexDataSize ^ std::rotr(eDword - key1, shift1);
}

DWORD decyptMode2(DWORD eDword, DWORD indexDataSize) {
    return indexDataSize ^ std::rotr(eDword - key2, shift2);
}

DWORD decyptMode3(DWORD eDword, DWORD indexDataSize) {
    return indexDataSize ^ std::rotr(eDword - key3, shift3);
}

DWORD decyptMode4(DWORD eDword, DWORD indexDataSize) {
    return indexDataSize ^ std::rotr(eDword - key4, shift4);
}

/**
 * @brief 使用基于文件大小的密钥解密16字节的加密文件头。
 * @param header 包含16字节头文件的vector引用。
 * @param file_size 整个MG2文件的总大小。
 */
void decrypt_header(std::vector<uint8_t>& orgHeader, size_t file_size) {
    std::vector<uint8_t> header = orgHeader;
    if (header.size() < 16) return;
    uint8_t key = ((file_size >> 8) & 0xFF) + ((file_size >> 24) & 0xFF);
    uint8_t increment = (file_size & 0xFF) + ((file_size >> 16) & 0xFF);
    std::cout << "Encrypted header detected, decrypting..." << std::endl;
    for (int i = 0; i < 16; ++i) {
        header[i] ^= key;
        key += increment;
    }
    if (memcmp(header.data(), "MICO", 4) == 0) {
        orgHeader = header;
        return;
    }
    header = orgHeader;

    uint32_t decryptMode = 0;
    for (uint32_t i = 0; i <= 0xC; i++) {
        DWORD eDword = read<DWORD>(header.data() + i);
        DWORD dDword;
        if (decryptMode) {
            dDword = decyptMode1(eDword, file_size);
            decryptMode = 0;
        }
        else {
            dDword = decyptMode2(eDword, file_size);
            decryptMode = 1;
        }
        write<DWORD>(header.data() + i, dDword);
    }
    if (memcmp(header.data(), "MICO", 4) == 0) {
        orgHeader = header;
        return;
    }
}

/**
 * @brief 使用V2算法解密数据块 (用于 'CG01' 版本)。
 * @param data 要解密的数据块的vector引用。
 */
void decrypt_v2_data(std::vector<uint8_t>& data) {
    if (data.empty()) return;
    uint32_t length = data.size();
    uint8_t key0 = length & 0xFF;
    size_t threshold = std::min((size_t)25, data.size());
    for (size_t i = 0; i < threshold; ++i) {
        data[i] ^= (uint8_t)(key0 + i);
    }
}

/**
 * @brief 使用V3算法加密或解密数据块 (用于明文头的 'CG02')。
 * @param data 数据块的vector引用。
 * @param key_seed 生成密钥的种子值 (总是主图像的长度)。
 */
void crypt_v3_data(std::vector<uint8_t>& data, uint32_t key_seed) {
    if (data.empty()) return;
    uint8_t key0 = (key_seed >> 1) & 0xFF;
    uint8_t key1 = ((key_seed & 1) + (key_seed >> 3)) & 0xFF;
    for (size_t pos = 0; pos < data.size(); ++pos) {
        uint8_t encryption_byte = ((pos >> 4) ^ (uint8_t)(pos + key0) ^ key1);
        data[pos] ^= encryption_byte;
    }
}

/**
 * @brief 使用'CG02'的备用算法解密数据块 (用于加密头的 'CG02')。
 * @param data 数据块的vector引用。
 * @param key_seed 生成密钥的种子值 (总是主图像的长度)。
 */
void decrypt_cg02_alt_data(std::vector<uint8_t>& data, uint32_t key_seed) {
    if (data.empty()) return;
    uint8_t key = ((key_seed >> 8) & 0xFF) + ((key_seed >> 24) & 0xFF);
    uint8_t increment = (key_seed & 0xFF) + ((key_seed >> 16) & 0xFF);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key;
        key += increment;
    }
}

void decrypt_cg03_data(std::vector<uint8_t>& data, uint32_t key_seed) {
    uint32_t data_offset = 0;
    uint32_t decryptMode = 0;
    do {
        DWORD eDword = read<DWORD>(data.data() + data_offset);
        DWORD dDword;
        if (decryptMode) {
            dDword = decyptMode3(eDword, key_seed);
            decryptMode = 0;
        }
        else {
            dDword = decyptMode4(eDword, key_seed);
            decryptMode = 1;
        }
        write<DWORD>(data.data() + data_offset, dDword);
        data_offset++;
    } while (data_offset <= data.size() - 4);
}

/**
 * @brief 垂直翻转图像。
 * @param image_pixels 指向原始像素数据的指针。
 * @param width 图像宽度。
 * @param height 图像高度。
 * @param channels 每像素的通道数。
 */
void flip_image_vertically(unsigned char* image_pixels, int width, int height, int channels) {
    if (!image_pixels) return;
    int row_size = width * channels;
    std::vector<unsigned char> temp_row(row_size);
    for (int y = 0; y < height / 2; ++y) {
        unsigned char* top_row = image_pixels + y * row_size;
        unsigned char* bottom_row = image_pixels + (height - 1 - y) * row_size;
        memcpy(temp_row.data(), top_row, row_size);
        memcpy(top_row, bottom_row, row_size);
        memcpy(bottom_row, temp_row.data(), row_size);
    }
}

// --- 转换函数 ---

/**
 * @brief 将MG2文件转换为PNG文件。
 * @param input_path 源MG2文件的路径。
 * @param output_path 目标PNG文件的路径。
 * @return 成功返回true，失败返回false。
 */
bool convert_mg2_to_png(const fs::path& input_path, const fs::path& output_path) {
    std::cout << "--- Decoding MG2 to PNG ---" << std::endl;
    std::ifstream file(input_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open input file: " << wide2Ascii(input_path) << std::endl;
        return false;
    }
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> header(16);
    file.read(reinterpret_cast<char*>(header.data()), 16);
    if (file.gcount() < 16) {
        std::cerr << "ERROR: Input file is too small to be a valid MG2 file." << std::endl;
        return false;
    }

    bool header_was_encrypted = false;
    if (memcmp(header.data(), "MICOCG", 6) != 0) {
        header_was_encrypted = true;
        decrypt_header(header, file_size);
    }

    if (memcmp(header.data(), "MICO", 4) != 0) {
        std::cerr << "ERROR: Invalid file signature. Not a valid MG2 file." << std::endl;
        return false;
    }

    std::string version(reinterpret_cast<char*>(header.data()) + 4, 4);
    std::cout << "File version: " << version << std::endl;

    uint32_t image_length = *reinterpret_cast<uint32_t*>(&header[8]);
    uint32_t alpha_length = *reinterpret_cast<uint32_t*>(&header[12]);
    std::cout << "Main image data length: " << image_length << " bytes" << std::endl;
    std::cout << "Alpha channel data length: " << alpha_length << " bytes" << std::endl;

    std::vector<uint8_t> main_image_data(image_length);
    file.read(reinterpret_cast<char*>(main_image_data.data()), image_length);
    if (file.gcount() < image_length) {
        std::cerr << "ERROR: Failed to read main image data block." << std::endl;
        return false;
    }

    std::vector<uint8_t> alpha_image_data(alpha_length);
    if (alpha_length > 0) {
        file.read(reinterpret_cast<char*>(alpha_image_data.data()), alpha_length);
        if (file.gcount() < alpha_length) {
            std::cerr << "ERROR: Failed to read alpha channel data block." << std::endl;
            return false;
        }
    }
    file.close();

    if (version == "CG01") {
        std::cout << "Applying V2 ('CG01') decryption algorithm..." << std::endl;
        decrypt_v2_data(main_image_data);
        if (alpha_length > 0) decrypt_v2_data(alpha_image_data);
    }
    else if (version == "CG02") {
        if (header_was_encrypted) {
            std::cout << "Applying alternate 'CG02' decryption (encrypted header)..." << std::endl;
            decrypt_cg02_alt_data(main_image_data, image_length);
            if (alpha_length > 0) decrypt_cg02_alt_data(alpha_image_data, image_length);
        }
        else {
            std::cout << "Applying standard V3 ('CG02') decryption (plaintext header)..." << std::endl;
            crypt_v3_data(main_image_data, image_length);
            if (alpha_length > 0) crypt_v3_data(alpha_image_data, image_length);
        }
    }
    else if (version == "CG03") {
        decrypt_cg03_data(main_image_data, image_length);
        if (alpha_length > 0) decrypt_cg03_data(alpha_image_data, image_length);
    }
    else {
        std::cerr << "ERROR: Unsupported version '" << version << "'. Only 'CG01' and 'CG02' are supported." << std::endl;
        return false;
    }

    int width, height, channels;
    unsigned char* main_pixels = stbi_load_from_memory(main_image_data.data(), main_image_data.size(), &width, &height, &channels, 0);
    if (!main_pixels) {
        std::cerr << "ERROR: Failed to decode main image data. Reason: " << stbi_failure_reason() << std::endl;
        return false;
    }
    std::cout << "Main image decoded successfully: " << width << "x" << height << ", " << channels << " channels" << std::endl;

    std::vector<unsigned char> final_pixels(width * height * 4);
    unsigned char* alpha_pixels = nullptr;
    if (alpha_length > 0) {
        int alpha_w, alpha_h, alpha_c;
        alpha_pixels = stbi_load_from_memory(alpha_image_data.data(), alpha_image_data.size(), &alpha_w, &alpha_h, &alpha_c, 1);
        if (!alpha_pixels) {
            std::cerr << "WARNING: Failed to decode alpha channel, ignoring it. Reason: " << stbi_failure_reason() << std::endl;
        }
        else {
            std::cout << "Alpha channel decoded successfully: " << alpha_w << "x" << alpha_h << std::endl;
            if (alpha_w != width || alpha_h != height) {
                std::cerr << "WARNING: Alpha channel dimensions mismatch, ignoring it." << std::endl;
                stbi_image_free(alpha_pixels);
                alpha_pixels = nullptr;
            }
        }
    }

    std::cout << "Merging RGB and Alpha channels..." << std::endl;
    for (int i = 0; i < width * height; ++i) {
        final_pixels[i * 4 + 0] = main_pixels[i * channels + 0];
        final_pixels[i * 4 + 1] = main_pixels[i * channels + 1];
        final_pixels[i * 4 + 2] = main_pixels[i * channels + 2];
        final_pixels[i * 4 + 3] = alpha_pixels ? alpha_pixels[i] : (channels == 4 ? main_pixels[i * 4 + 3] : 255);
    }
    stbi_image_free(main_pixels);
    if (alpha_pixels) stbi_image_free(alpha_pixels);

    std::cout << "Flipping image vertically..." << std::endl;
    flip_image_vertically(final_pixels.data(), width, height, 4);

    if (stbi_write_png(wide2Ascii(output_path).c_str(), width, height, 4, final_pixels.data(), width * 4)) {
        std::cout << "Success! File saved to " << wide2Ascii(output_path) << std::endl;
        return true;
    }
    else {
        std::cerr << "ERROR: Failed to write final PNG file." << std::endl;
        return false;
    }
}

/**
 * @brief 将32位PNG文件转换为MG2文件。
 * @param input_path 源PNG文件的路径。
 * @param output_path 目标MG2文件的路径。
 * @return 成功返回true，失败返回false。
 */
bool convert_png_to_mg2(const fs::path& input_path, const fs::path& output_path) {
    std::cout << "--- Encoding PNG to MG2 ---" << std::endl;
    std::cout << "Loading PNG file: " << wide2Ascii(input_path) << std::endl;
    int width, height, channels;
    unsigned char* pixels = stbi_load(wide2Ascii(input_path).c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        std::cerr << "ERROR: Failed to load PNG file. Reason: " << stbi_failure_reason() << std::endl;
        return false;
    }
    std::cout << "Image loaded successfully: " << width << "x" << height << std::endl;

    std::cout << "Separating RGB and Alpha channels..." << std::endl;
    std::vector<unsigned char> rgb_data(width * height * 3);
    std::vector<unsigned char> alpha_data(width * height);
    for (int i = 0; i < width * height; ++i) {
        rgb_data[i * 3 + 0] = pixels[i * 4 + 0]; // R
        rgb_data[i * 3 + 1] = pixels[i * 4 + 1]; // G
        rgb_data[i * 3 + 2] = pixels[i * 4 + 2]; // B
        alpha_data[i] = pixels[i * 4 + 3]; // A
    }
    stbi_image_free(pixels);

    std::cout << "Flipping images vertically for 'CG02' format..." << std::endl;
    flip_image_vertically(rgb_data.data(), width, height, 3);
    flip_image_vertically(alpha_data.data(), width, height, 1);

    std::cout << "Re-encoding separated data to in-memory PNGs..." << std::endl;
    int main_len_png = 0;
    unsigned char* main_png_mem = stbi_write_png_to_mem(rgb_data.data(), 0, width, height, 3, &main_len_png);
    if (!main_png_mem) {
        std::cerr << "ERROR: Failed to encode RGB data to PNG." << std::endl;
        return false;
    }
    std::vector<uint8_t> main_image_block(main_png_mem, main_png_mem + main_len_png);
    stbi_image_free(main_png_mem);

    int alpha_len_png = 0;
    unsigned char* alpha_png_mem = stbi_write_png_to_mem(alpha_data.data(), 0, width, height, 1, &alpha_len_png);
    if (!alpha_png_mem) {
        std::cerr << "ERROR: Failed to encode Alpha data to PNG." << std::endl;
        return false;
    }
    std::vector<uint8_t> alpha_image_block(alpha_png_mem, alpha_png_mem + alpha_len_png);
    stbi_image_free(alpha_png_mem);

    uint32_t image_length = main_image_block.size();
    uint32_t alpha_length = alpha_image_block.size();
    std::cout << "In-memory main image PNG size: " << image_length << " bytes" << std::endl;
    std::cout << "In-memory alpha image PNG size: " << alpha_length << " bytes" << std::endl;

    std::cout << "Encrypting data blocks with V3 algorithm..." << std::endl;
    crypt_v3_data(main_image_block, image_length);
    crypt_v3_data(alpha_image_block, image_length);

    std::cout << "Building 'MICOCG02' file header..." << std::endl;
    std::vector<uint8_t> header(16);
    memcpy(header.data(), "MICOCG02", 8);
    *reinterpret_cast<uint32_t*>(&header[8]) = image_length;
    *reinterpret_cast<uint32_t*>(&header[12]) = alpha_length;

    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "ERROR: Cannot create output file: " << wide2Ascii(output_path) << std::endl;
        return false;
    }
    outfile.write(reinterpret_cast<const char*>(header.data()), 16);
    outfile.write(reinterpret_cast<const char*>(main_image_block.data()), image_length);
    outfile.write(reinterpret_cast<const char*>(alpha_image_block.data()), alpha_length);
    outfile.close();

    std::cout << "Success! File saved to " << wide2Ascii(output_path) << std::endl;
    return true;
}

void print_usage(const fs::path& program_path) {
    std::string program_filename = wide2Ascii(program_path.filename());
    std::cout << "Made by julixian 2025.07.24" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << program_filename << " <command> <input_file> <output_file>" << std::endl << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  decode    Converts an MG2 file to a PNG file." << std::endl;
    std::cout << "  encode    Converts a PNG file to an MG2 file." << std::endl << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_filename << " decode character.mg2 character.png" << std::endl;
    std::cout << "  " << program_filename << " encode new_image.png new_image.mg2" << std::endl;
}

int wmain(int argc, wchar_t* argv[]) {

    SetConsoleOutputCP(CP_UTF8);

    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    std::wstring command = argv[1];
    const fs::path input_path = argv[2];
    const fs::path output_path = argv[3];

    bool success = false;
    if (command == L"decode") {
        success = convert_mg2_to_png(input_path, output_path);
    }
    else if (command == L"encode") {
        success = convert_png_to_mg2(input_path, output_path);
    }
    else {
        std::cerr << "ERROR: Unknown command '" << wide2Ascii(command) << "'." << std::endl << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return success ? 0 : 1;
}
