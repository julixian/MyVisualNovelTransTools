#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <Windows.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

import std;
namespace fs = std::filesystem;

extern "C" size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);
extern "C" size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);


#pragma pack(push, 1)
struct AkbHeader {
    uint32_t signature;
    uint16_t width;
    uint16_t height;
    uint32_t flags;
    uint8_t  background[4]; // BGRA
    int32_t  offsetX;
    int32_t  offsetY;
    int32_t  right;
    int32_t  bottom;
};
#pragma pack(pop)

// restore_delta 的逆向操作
void apply_delta_csharp_equivalent(std::vector<uint8_t>& pixels, int stride, int pixel_size) {
    if (pixels.empty()) return;

    // 逆向操作必须从后往前进行，以使用未被修改的原始值

    // 逆向：后续行
    for (size_t i = pixels.size() - 1; i >= stride; --i) {
        pixels[i] -= pixels[i - stride];
    }

    // 逆向：第一行
    for (int i = stride - 1; i >= pixel_size; --i) {
        pixels[i] -= pixels[i - pixel_size];
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::println("Usage: {} <input.png> <output.akb>", argv[0]);
        return 1;
    }

    std::filesystem::path input_path = argv[1];
    std::filesystem::path output_path = argv[2];

    try {
        // 1. 加载 PNG 文件
        std::println("Loading PNG file: {}", input_path.string());
        int width, height, channels;
        // 强制加载为4通道 (RGBA)
        unsigned char* png_data = stbi_load(input_path.string().c_str(), &width, &height, &channels, 4);
        if (!png_data) {
            throw std::runtime_error("Failed to load PNG file. It might be corrupted or not exist.");
        }
        std::println("  Dimensions: {}x{}", width, height);

        const int pixel_size = 4; // 32-bit BGRA
        size_t pixel_data_size = width * height * pixel_size;
        std::vector<uint8_t> pixels(png_data, png_data + pixel_data_size);
        stbi_image_free(png_data); // 释放 stb 加载的内存

        // 2. 颜色通道转换 (RGBA -> BGRA)
        std::println("Converting RGBA to BGRA...");
        for (size_t i = 0; i < pixel_data_size; i += 4) {
            std::swap(pixels[i], pixels[i + 2]); // Swap R and B
        }

        // 3. 应用差分编码
        std::println("Applying delta filter...");
        apply_delta_csharp_equivalent(pixels, width * pixel_size, pixel_size);

        // 4. 垂直翻转图像数据
        std::println("Flipping image data vertically for AKB storage...");
        std::vector<uint8_t> flipped_pixels(pixel_data_size);
        const int stride = width * pixel_size;
        for (int y = 0; y < height; ++y) {
            const uint8_t* src_row = pixels.data() + y * stride;
            uint8_t* dst_row = flipped_pixels.data() + (height - 1 - y) * stride;
            std::copy(src_row, src_row + stride, dst_row);
        }

        // 5. LZSS 压缩
        std::println("Compressing data with LZSS...");
        std::vector<uint8_t> compressed_data(pixel_data_size * 2);
        size_t compressed_size = lzss_compress(
            compressed_data.data(), compressed_data.size(),
            flipped_pixels.data(), flipped_pixels.size()
        );

        if (compressed_size == 0) {
            throw std::runtime_error("LZSS compression failed. The output buffer might be too small.");
        }
        compressed_data.resize(compressed_size); // 调整为实际压缩后的大小

        std::println("  Original size: {} bytes", pixel_data_size);
        std::println("  Compressed size: {} bytes", compressed_size);

        // 6. 构建 AKB 文件头
        AkbHeader header = {};
        header.signature = 0x20424B41; // 'AKB '
        header.width = static_cast<uint16_t>(width);
        header.height = static_cast<uint16_t>(height);

        // Flags for 32-bit BGRA:
        // bit 30 (0x40000000) = 0 for 32bpp
        // bit 31 (0x80000000) = 1 for alpha channel enabled
        header.flags = 0x80000000;

        // 背景设为透明黑
        header.background[0] = 0; // B
        header.background[1] = 0; // G
        header.background[2] = 0; // R
        header.background[3] = 0; // A

        // 使用全尺寸图像，无偏移
        header.offsetX = 0;
        header.offsetY = 0;
        header.right = width;
        header.bottom = height;

        // 7. 写入 AKB 文件
        std::println("Writing AKB file: {}", output_path.string());
        std::ofstream file(output_path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to create output file.");
        }

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
        file.close();

        std::println("Conversion successful!");

    }
    catch (const std::exception& e) {
        std::println("\nError: {}", e.what());
        return 1;
    }

    return 0;
}
