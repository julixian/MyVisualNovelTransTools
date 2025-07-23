#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <cstring>

// 定义STB库的实现宏
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// --- 加密与辅助函数 ---

/**
 * @brief 使用V3算法加密数据块 (XOR是其自身的逆运算)。
 * @param data 要加密的数据。
 * @param key_seed 用于生成密钥的种子值 (即 ImageLength)。
 */
void encrypt_v3_data(std::vector<uint8_t>& data, uint32_t key_seed) {
    if (data.empty()) return;
    uint8_t key0 = (key_seed >> 1) & 0xFF;
    uint8_t key1 = ((key_seed & 1) + (key_seed >> 3)) & 0xFF;
    for (size_t pos = 0; pos < data.size(); ++pos) {
        uint8_t encryption_byte = ((pos >> 4) ^ (pos + key0) ^ key1);
        data[pos] ^= encryption_byte;
    }
}

/**
 * @brief 垂直翻转图像。
 * @param image_pixels 指向像素数据的指针。
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

// --- 主转换逻辑 ---

bool convert_png_to_mg2(const std::string& input_path, const std::string& output_path) {
    // 1. 加载PNG文件，强制为4通道(RGBA)
    std::cout << "加载PNG文件: " << input_path << std::endl;
    int width, height, channels;
    unsigned char* pixels = stbi_load(input_path.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        std::cerr << "错误: 无法加载PNG文件。原因: " << stbi_failure_reason() << std::endl;
        return false;
    }
    std::cout << "图像加载成功: " << width << "x" << height << std::endl;

    // 2. 分离RGB和Alpha通道数据
    std::cout << "分离RGB和Alpha通道..." << std::endl;
    std::vector<unsigned char> rgb_data(width * height * 3);
    std::vector<unsigned char> alpha_data(width * height);
    for (int i = 0; i < width * height; ++i) {
        rgb_data[i * 3 + 0] = pixels[i * 4 + 0]; // R
        rgb_data[i * 3 + 1] = pixels[i * 4 + 1]; // G
        rgb_data[i * 3 + 2] = pixels[i * 4 + 2]; // B
        alpha_data[i] = pixels[i * 4 + 3]; // A
    }
    stbi_image_free(pixels); // 释放原始像素数据

    // 3. 垂直翻转图像 (CG02格式要求)
    std::cout << "垂直翻转图像..." << std::endl;
    flip_image_vertically(rgb_data.data(), width, height, 3);
    flip_image_vertically(alpha_data.data(), width, height, 1);

    // 4. 将分离后的数据重新编码为PNG格式 (在内存中)
    std::cout << "将分离的数据重新编码为内存中的PNG..." << std::endl;
    int main_len_png = 0;
    unsigned char* main_png_mem = stbi_write_png_to_mem(rgb_data.data(), width * 3, width, height, 3, &main_len_png);
    if (!main_png_mem) {
        std::cerr << "错误: 无法将RGB数据编码为PNG。" << std::endl;
        return false;
    }
    std::vector<uint8_t> main_image_block(main_png_mem, main_png_mem + main_len_png);
    stbi_image_free(main_png_mem);

    int alpha_len_png = 0;
    unsigned char* alpha_png_mem = stbi_write_png_to_mem(alpha_data.data(), width, width, height, 1, &alpha_len_png);
    if (!alpha_png_mem) {
        std::cerr << "错误: 无法将Alpha数据编码为PNG。" << std::endl;
        return false;
    }
    std::vector<uint8_t> alpha_image_block(alpha_png_mem, alpha_png_mem + alpha_len_png);
    stbi_image_free(alpha_png_mem);

    uint32_t image_length = main_image_block.size();
    uint32_t alpha_length = alpha_image_block.size();
    std::cout << "主图像PNG大小: " << image_length << " 字节" << std::endl;
    std::cout << "Alpha图像PNG大小: " << alpha_length << " 字节" << std::endl;

    // 5. 加密数据块
    std::cout << "使用V3算法加密数据块..." << std::endl;
    encrypt_v3_data(main_image_block, image_length);
    encrypt_v3_data(alpha_image_block, image_length); // Alpha也用主图像长度做密钥

    // 6. 构建MG2文件头
    std::cout << "构建 'MICOCG02' 文件头..." << std::endl;
    std::vector<uint8_t> header(16);
    memcpy(header.data(), "MICOCG02", 8);
    *reinterpret_cast<uint32_t*>(&header[8]) = image_length; // 小端序写入
    *reinterpret_cast<uint32_t*>(&header[12]) = alpha_length; // 小端序写入

    // 7. 写入最终的MG2文件
    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "错误: 无法创建输出文件 " << output_path << std::endl;
        return false;
    }
    outfile.write(reinterpret_cast<const char*>(header.data()), 16);
    outfile.write(reinterpret_cast<const char*>(main_image_block.data()), image_length);
    outfile.write(reinterpret_cast<const char*>(alpha_image_block.data()), alpha_length);
    outfile.close();

    std::cout << "成功！文件已保存到 " << output_path << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "PNG to MG2 转换工具" << std::endl;
        std::cout << "用法: " << argv[0] << " <输入png文件> <输出mg2文件>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    if (convert_png_to_mg2(input_file, output_file)) {
        return 0;
    }
    else {
        return 1;
    }
}
