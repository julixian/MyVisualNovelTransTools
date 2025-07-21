#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ----------------------------------------------------------------------------
extern "C" size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);
extern "C" size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);
// ----------------------------------------------------------------------------


#pragma pack(push, 1)
struct GsPicHeader {
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t magic;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint8_t  reserved[12];
};
#pragma pack(pop)

void convert_bgra_to_rgba(std::vector<uint8_t>& pixels, int width, int height) {
    for (size_t i = 0; i < width * height * 4; i += 4) {
        std::swap(pixels[i], pixels[i + 2]);
    }
}

void convert_bgr_to_rgb(std::vector<uint8_t>& pixels, int width, int height) {
    for (size_t i = 0; i < width * height * 3; i += 3) {
        std::swap(pixels[i], pixels[i + 2]);
    }
}

void convert_gspic_to_png(const std::string& input_path, const std::string& output_path) {
    std::ifstream file(input_path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("无法打开输入文件: " + input_path);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> file_buffer(size);
    if (!file.read(reinterpret_cast<char*>(file_buffer.data()), size)) {
        throw std::runtime_error("无法读取文件内容: " + input_path);
    }
    file.close();

    if (file_buffer.size() < sizeof(GsPicHeader)) {
        throw std::runtime_error("文件太小，不是有效的 GsPic 文件。");
    }
    const GsPicHeader* header = reinterpret_cast<const GsPicHeader*>(file_buffer.data());

    std::cout << "GsPic 文件头信息:" << std::endl;
    std::cout << "  - 压缩大小: " << header->compressed_size << " 字节" << std::endl;
    std::cout << "  - 解压大小: " << header->uncompressed_size << " 字节" << std::endl;
    std::cout << "  - 魔法数: 0x" << std::hex << header->magic << std::dec << std::endl;
    std::cout << "  - 尺寸: " << header->width << "x" << header->height << std::endl;
    std::cout << "  - 位深度: " << header->bpp << " bpp" << std::endl;

    if (header->magic != 0x0F53FFFF) {
        std::cerr << "[警告] 文件的魔法数与预期的 0x0F53FFFF 不符。" << std::endl;
    }

    const int channels = header->bpp / 8;
    if (channels != 3 && channels != 4) {
        throw std::runtime_error("不支持的位深度: " + std::to_string(header->bpp) + "。只支持 24 和 32 位。");
    }

    if (header->uncompressed_size != header->width * header->height * channels) {
        std::cerr << "[警告] 文件头中的解压大小与根据尺寸计算的大小不符。" << std::endl;
    }

    uint8_t* compressed_data_ptr = file_buffer.data() + sizeof(GsPicHeader);
    std::vector<uint8_t> decompressed_pixels(header->uncompressed_size);

    size_t actual_decompressed_size = lzss_decompress(
        decompressed_pixels.data(),
        header->uncompressed_size,
        compressed_data_ptr,
        header->compressed_size
    );

    if (actual_decompressed_size == 0) {
        throw std::runtime_error("LZSS 解压缩失败！");
    }
    std::cout << "解压缩完成，得到 " << actual_decompressed_size << " 字节数据。" << std::endl;

    if (channels == 4) {
        bool all_alpha_is_zero = true;
        for (size_t i = 3; i < decompressed_pixels.size(); i += 4) {
            if (decompressed_pixels[i] != 0) {
                all_alpha_is_zero = false;
                break;
            }
        }

        if (all_alpha_is_zero) {
            std::cout << "检测到32bpp图像的Alpha通道为空，将作为24bpp RGB处理..." << std::endl;
            std::vector<uint8_t> rgb_pixels(header->width * header->height * 3);
            for (size_t i = 0, j = 0; i < decompressed_pixels.size(); i += 4, j += 3) {
                rgb_pixels[j] = decompressed_pixels[i];
                rgb_pixels[j + 1] = decompressed_pixels[i + 1];
                rgb_pixels[j + 2] = decompressed_pixels[i + 2];
            }
            convert_bgr_to_rgb(rgb_pixels, header->width, header->height);
            const int stride_in_bytes = header->width * 3;
            if (!stbi_write_png(output_path.c_str(), header->width, header->height, 3, rgb_pixels.data(), stride_in_bytes)) {
                throw std::runtime_error("使用 stb 库写入 24-bit PNG 文件失败: " + output_path);
            }
        }
        else {
            std::cout << "执行 BGRA -> RGBA 颜色通道转换..." << std::endl;
            convert_bgra_to_rgba(decompressed_pixels, header->width, header->height);
            const int stride_in_bytes = header->width * channels;
            if (!stbi_write_png(output_path.c_str(), header->width, header->height, channels, decompressed_pixels.data(), stride_in_bytes)) {
                throw std::runtime_error("使用 stb 库写入 32-bit PNG 文件失败: " + output_path);
            }
        }
    }
    else if (channels == 3) {
        std::cout << "执行 BGR -> RGB 颜色通道转换..." << std::endl;
        convert_bgr_to_rgb(decompressed_pixels, header->width, header->height);
        const int stride_in_bytes = header->width * channels;
        if (!stbi_write_png(output_path.c_str(), header->width, header->height, channels, decompressed_pixels.data(), stride_in_bytes)) {
            throw std::runtime_error("使用 stb 库写入 24-bit PNG 文件失败: " + output_path);
        }
    }
    std::cout << "\n成功！文件已转换为: " << output_path << std::endl;
}

void convert_png_to_gspic(const std::string& input_png_path, const std::string& output_gspic_path, const std::string& template_gspic_path) {
    std::cout << "读取模板文件: " << template_gspic_path << std::endl;
    std::ifstream template_file(template_gspic_path, std::ios::binary);
    if (!template_file) {
        throw std::runtime_error("无法打开模板文件: " + template_gspic_path);
    }
    GsPicHeader template_header;
    template_file.read(reinterpret_cast<char*>(&template_header), sizeof(GsPicHeader));
    if (!template_file) {
        throw std::runtime_error("读取模板文件头失败。");
    }
    template_file.close();

    const int target_bpp = template_header.bpp;
    if (target_bpp != 24 && target_bpp != 32) {
        throw std::runtime_error("模板文件不是 24bpp 或 32bpp，不支持。");
    }
    std::cout << "模板信息: 目标位深度为 " << target_bpp << "bpp" << std::endl;

    std::cout << "读取输入 PNG: " << input_png_path << std::endl;
    int png_width, png_height, png_channels;
    auto stbi_deleter = [](unsigned char* p) { if (p) stbi_image_free(p); };
    std::unique_ptr<unsigned char, decltype(stbi_deleter)> png_pixels(
        stbi_load(input_png_path.c_str(), &png_width, &png_height, &png_channels, 0),
        stbi_deleter
    );

    if (!png_pixels) {
        throw std::runtime_error("使用 stb 库读取 PNG 文件失败: " + input_png_path);
    }
    std::cout << "PNG 信息: " << png_width << "x" << png_height << ", " << png_channels << " 通道" << std::endl;

    const int target_channels = target_bpp / 8;
    std::vector<uint8_t> gspic_pixel_data(png_width * png_height * target_channels);

    if (target_channels == 3) { // 目标是 24bpp BGR
        std::cout << "转换为 24bpp BGR 格式..." << std::endl;
        for (int i = 0; i < png_width * png_height; ++i) {
            uint8_t* src = png_pixels.get() + i * png_channels;
            uint8_t* dst = gspic_pixel_data.data() + i * 3;
            dst[0] = src[2]; // B
            dst[1] = src[1]; // G
            dst[2] = src[0]; // R
        }
    }
    else { // 目标是 32bpp BGRA
        std::cout << "转换为 32bpp BGRA 格式..." << std::endl;

        // 检查 Alpha 通道是否全部为 0xFF (不透明)
        bool all_alpha_opaque = true;
        if (png_channels == 4) {
            for (int i = 0; i < png_width * png_height; ++i) {
                if (png_pixels.get()[i * 4 + 3] != 255) {
                    all_alpha_opaque = false;
                    break;
                }
            }
        }

        if (all_alpha_opaque) {
            std::cout << "PNG Alpha 通道完全不透明，根据规则将 GsPic Alpha 设置为 0..." << std::endl;
        }

        for (int i = 0; i < png_width * png_height; ++i) {
            uint8_t* src = png_pixels.get() + i * png_channels;
            uint8_t* dst = gspic_pixel_data.data() + i * 4;
            dst[0] = src[2]; // B
            dst[1] = src[1]; // G
            dst[2] = src[0]; // R
            if (all_alpha_opaque) {
                dst[3] = 0; // 特殊规则：全不透明则设为0
            }
            else {
                dst[3] = (png_channels == 4) ? src[3] : 255; // 如果源是RGB，则Alpha为255
            }
        }
    }

    std::cout << "正在使用 LZSS 压缩数据..." << std::endl;
    size_t uncompressed_size = gspic_pixel_data.size();
    std::vector<uint8_t> compressed_data(uncompressed_size * 1.2 + 12);
    size_t compressed_size = lzss_compress(
        compressed_data.data(),
        compressed_data.size(),
        gspic_pixel_data.data(),
        uncompressed_size
    );
    if (compressed_size == 0) {
        throw std::runtime_error("LZSS 压缩失败！");
    }
    std::cout << "压缩完成: " << uncompressed_size << " -> " << compressed_size << " 字节" << std::endl;

    GsPicHeader new_header = template_header;
    new_header.width = png_width;
    new_header.height = png_height;
    new_header.uncompressed_size = uncompressed_size;
    new_header.compressed_size = compressed_size;
    // bpp, magic, version, reserved 已从模板继承

    std::cout << "正在写入输出文件: " << output_gspic_path << std::endl;
    std::ofstream out_file(output_gspic_path, std::ios::binary);
    if (!out_file) {
        throw std::runtime_error("无法创建输出文件: " + output_gspic_path);
    }
    out_file.write(reinterpret_cast<const char*>(&new_header), sizeof(GsPicHeader));
    out_file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_size);
    out_file.close();

    std::cout << "\n成功！文件已转换为: " << output_gspic_path << std::endl;
}


int main(int argc, char* argv[]) {
    auto print_usage = [&]() {
        std::cout << "Made by julixian 2025.07.21" << std::endl;
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  Mode 1 (GsPic -> PNG): " << argv[0] << " gspic2png <input.gspic> <output.png>" << std::endl;
        std::cerr << "  Mode 2 (PNG -> GsPic): " << argv[0] << " png2gspic <input.png> <output.gspic> <template(org).gspic>" << std::endl;
        };

    if (argc < 4) {
        print_usage();
        return 1;
    }

    std::string mode = argv[1];

    try {
        if (mode == "gspic2png") {
            if (argc != 4) {
                print_usage();
                return 1;
            }
            convert_gspic_to_png(argv[2], argv[3]);
        }
        else if (mode == "png2gspic") {
            if (argc != 5) {
                print_usage();
                return 1;
            }
            convert_png_to_gspic(argv[2], argv[3], argv[4]);
        }
        else {
            std::cerr << "Error：Unknow mode '" << mode << "'" << std::endl;
            print_usage();
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "\n[错误] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
