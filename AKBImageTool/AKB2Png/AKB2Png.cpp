#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <Windows.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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

enum class PixelFormat {
    BGR24,
    BGR32,
    BGRA32,
    UNKNOWN
};

void restore_delta(std::vector<uint8_t>& pixels, int stride, int pixel_size) {
    if (pixels.empty() || stride == 0) return;

    // 还原每一行内部的差值: 当前像素 += 左边像素
    for (size_t y = 0; y < pixels.size() / stride; ++y) {
        size_t row_start = y * stride;
        for (int i = pixel_size; i < stride; ++i) {
            pixels[row_start + i] += pixels[row_start + i - pixel_size];
        }
    }

    // 还原行与行之间的差值: 当前行像素 += 上一行像素
    for (size_t i = stride; i < pixels.size(); ++i) {
        pixels[i] += pixels[i - stride];
    }
}

// C#代码中的RestoreDelta实现是混合的，这里也进行修正以严格匹配
void restore_delta_csharp_equivalent(std::vector<uint8_t>& pixels, int stride, int pixel_size) {
    if (pixels.empty()) return;

    // C# 版本: 还原第一行
    for (int i = pixel_size; i < stride; ++i) {
        pixels[i] += pixels[i - pixel_size];
    }

    // C# 版本: 还原后续行
    for (size_t i = stride; i < pixels.size(); ++i) {
        pixels[i] += pixels[i - stride];
    }
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::println("Usage: {} <input.akb> <output.png>", argv[0]);
        return 1;
    }

    std::filesystem::path input_path = argv[1];
    std::filesystem::path output_path = argv[2];

    try {
        std::println("Opening AKB file: {}", input_path.string());
        std::ifstream file(input_path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Failed to open input file.");

        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (file_size < sizeof(AkbHeader)) throw std::runtime_error("File is too small to be a valid AKB file.");

        AkbHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        const uint32_t AKB_SIGNATURE = 0x20424B41; // 'AKB '
        const uint32_t AKB_INCREMENTAL_SIGNATURE = 0x2B424B41; // 'AKB+'

        if (header.signature != AKB_SIGNATURE) {
            if (header.signature == AKB_INCREMENTAL_SIGNATURE) {
                throw std::runtime_error("This is an incremental AKB file, which is not supported by this tool.");
            }
            throw std::runtime_error("Invalid AKB file signature.");
        }

        const int32_t inner_width = header.right - header.offsetX;
        const int32_t inner_height = header.bottom - header.offsetY;
        const int32_t bpp = (header.flags & 0x40000000) ? 24 : 32;
        const int pixel_size = bpp / 8;

        if (inner_width <= 0 || inner_height <= 0) {
            throw std::runtime_error("Invalid inner image dimensions.");
        }

        PixelFormat format = PixelFormat::UNKNOWN;
        int num_components = 0;

        if (bpp == 24) {
            format = PixelFormat::BGR24;
            num_components = 3;
        }
        else if (bpp == 32) {
            num_components = 4;
            format = (header.flags & 0x80000000) ? PixelFormat::BGRA32 : PixelFormat::BGR32;
        }

        if (format == PixelFormat::UNKNOWN) throw std::runtime_error("Unsupported pixel format.");

        std::println("--- AKB Info ---");
        std::println("  Dimensions: {}x{}", header.width, header.height);
        std::println("  Inner Rect: {}x{} at ({}, {})", inner_width, inner_height, header.offsetX, header.offsetY);
        std::println("  BPP: {}", bpp);
        std::println("----------------");

        size_t compressed_size = file_size - sizeof(AkbHeader);
        std::vector<uint8_t> compressed_data(compressed_size);
        file.read(reinterpret_cast<char*>(compressed_data.data()), compressed_size);
        file.close();

        std::println("Decompressing data...");
        size_t decompressed_size_expected = inner_width * inner_height * pixel_size;
        std::vector<uint8_t> decompressed_pixels(decompressed_size_expected);

        size_t actual_decompressed_size = lzss_decompress(
            decompressed_pixels.data(), decompressed_pixels.size(),
            compressed_data.data(), compressed_data.size()
        );

        if (actual_decompressed_size == 0) throw std::runtime_error("LZSS decompression failed.");
        if (actual_decompressed_size != decompressed_size_expected) {
            std::println("Warning: Decompressed size ({}) does not match expected size ({}).",
                actual_decompressed_size, decompressed_size_expected);
        }

        // ====================================================================
        // ======================== CRITICAL FIX START ========================
        // ====================================================================

        // C# code reads rows in reverse order, effectively flipping the image vertically during decompression.
        // Since we decompress in one go, our `decompressed_pixels` buffer has its rows inverted.
        // We MUST flip the rows vertically BEFORE applying the delta filter.

        std::vector<uint8_t> inner_pixels(decompressed_size_expected);
        const int inner_stride = inner_width * pixel_size;
        for (int y = 0; y < inner_height; ++y) {
            // Source row is from the bottom of the decompressed buffer
            const uint8_t* src_row = decompressed_pixels.data() + (inner_height - 1 - y) * inner_stride;
            // Destination row is from the top of the correctly ordered buffer
            uint8_t* dst_row = inner_pixels.data() + y * inner_stride;
            std::copy(src_row, src_row + inner_stride, dst_row);
        }

        // Now `inner_pixels` has the correct top-to-bottom row order, just like in the C# program
        // after its decompression loop. We can now safely apply the delta filter.

        std::println("Restoring delta filter...");
        restore_delta_csharp_equivalent(inner_pixels, inner_stride, pixel_size);

        // ====================================================================
        // ========================= CRITICAL FIX END =========================
        // ====================================================================


        std::println("Compositing final image...");
        std::vector<uint8_t> final_image(header.width * header.height * num_components);

        for (int y = 0; y < header.height; ++y) {
            for (int x = 0; x < header.width; ++x) {
                size_t dst_idx = (y * header.width + x) * num_components;
                final_image[dst_idx + 0] = header.background[2]; // R
                final_image[dst_idx + 1] = header.background[1]; // G
                final_image[dst_idx + 2] = header.background[0]; // B
                if (num_components == 4) final_image[dst_idx + 3] = header.background[3]; // A
            }
        }

        for (int y = 0; y < inner_height; ++y) {
            for (int x = 0; x < inner_width; ++x) {
                int dst_x = header.offsetX + x;
                int dst_y = header.offsetY + y;

                if (dst_x >= 0 && dst_x < header.width && dst_y >= 0 && dst_y < header.height) {
                    // Since `inner_pixels` is now correctly ordered, we do NOT flip the source y-coordinate here.
                    size_t src_idx = (y * inner_width + x) * pixel_size;
                    size_t dst_idx = (dst_y * header.width + dst_x) * num_components;

                    final_image[dst_idx + 0] = inner_pixels[src_idx + 2]; // R
                    final_image[dst_idx + 1] = inner_pixels[src_idx + 1]; // G
                    final_image[dst_idx + 2] = inner_pixels[src_idx + 0]; // B

                    if (num_components == 4) {
                        final_image[dst_idx + 3] = (pixel_size == 4) ? inner_pixels[src_idx + 3] : 255;
                    }
                }
            }
        }

        std::println("Writing PNG file: {}", output_path.string());
        int success = stbi_write_png(
            output_path.string().c_str(), header.width, header.height,
            num_components, final_image.data(), header.width * num_components
        );

        if (!success) throw std::runtime_error("Failed to write PNG file.");

        std::println("Conversion successful!");

    }
    catch (const std::exception& e) {
        std::println("\nError: {}", e.what());
        return 1;
    }

    return 0;
}
