#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <png.h>

class PngReader {
private:
    png_structp png_ptr;
    png_infop info_ptr;
    FILE* fp;
    std::vector<png_bytep> row_pointers;
    int width, height;
    bool has_alpha;

public:
    PngReader() : png_ptr(nullptr), info_ptr(nullptr), fp(nullptr) {}

    ~PngReader() {
        cleanup();
    }

    bool open(const char* filename) {
        fp = fopen(filename, "rb");
        if (!fp) {
            std::cerr << "Cannot open file: " << filename << std::endl;
            return false;
        }

        unsigned char header[8];
        if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
            std::cerr << "Not a valid PNG file" << std::endl;
            cleanup();
            return false;
        }

        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png_ptr) {
            cleanup();
            return false;
        }

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            cleanup();
            return false;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            cleanup();
            return false;
        }

        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);
        png_read_info(png_ptr, info_ptr);

        width = png_get_image_width(png_ptr, info_ptr);
        height = png_get_image_height(png_ptr, info_ptr);
        png_byte color_type = png_get_color_type(png_ptr, info_ptr);
        png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        if (bit_depth == 16)
            png_set_strip_16(png_ptr);

        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png_ptr);

        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png_ptr);

        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png_ptr);

        if (color_type == PNG_COLOR_TYPE_RGB ||
            color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png_ptr);

        png_read_update_info(png_ptr, info_ptr);

        row_pointers.resize(height);
        for (int y = 0; y < height; y++) {
            row_pointers[y] = new png_byte[png_get_rowbytes(png_ptr, info_ptr)];
        }

        png_read_image(png_ptr, row_pointers.data());
        return true;
    }

    bool convertToData(const char* output_filename) {
        std::ofstream out(output_filename, std::ios::binary);
        if (!out) {
            std::cerr << "Cannot create output file" << std::endl;
            return false;
        }

        int ctl = 1;  // 初始控制字节为1

        for (int y = height - 1; y >= 0; y--) {  
            png_bytep row = row_pointers[y];
            for (int x = 0; x < width; x++) {
                // 当控制字节为1时，写入新的控制字节
                if (ctl == 1) {
                    out.put(0);  // 写入0作为控制字节
                    ctl = 0x100;
                }

                // RGBA转换为BGRA
                uint8_t pixel[4];
                pixel[0] = row[x * 4 + 2]; // B
                pixel[1] = row[x * 4 + 1]; // G
                pixel[2] = row[x * 4 + 0]; // R
                pixel[3] = row[x * 4 + 3]; // A

                // 写入像素数据
                out.write(reinterpret_cast<char*>(pixel), 4);

                // 更新控制字节
                ctl >>= 1;
            }
        }

        return true;
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    void cleanup() {
        for (auto& row : row_pointers) {
            delete[] row;
        }
        row_pointers.clear();

        if (png_ptr) {
            if (info_ptr) {
                png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            }
            else {
                png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            }
        }
        if (fp) {
            fclose(fp);
            fp = nullptr;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Made by julixian 2025.03.14" << std::endl;
        std::cout << "Usage: " << argv[0] << " <input.png> <output.data>" << std::endl;
        return 1;
    }

    PngReader reader;
    if (!reader.open(argv[1])) {
        std::cerr << "Failed to open PNG file" << std::endl;
        return 1;
    }

    std::cout << "Converting PNG (" << reader.getWidth() << "x" << reader.getHeight()
        << ") to data format..." << std::endl;

    if (!reader.convertToData(argv[2])) {
        std::cerr << "Failed to convert file" << std::endl;
        return 1;
    }

    std::cout << "Conversion successful!" << std::endl;
    return 0;
}
