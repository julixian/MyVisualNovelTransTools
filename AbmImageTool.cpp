#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <png.h>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct BMHeader {
    char magic[2];          // 'BM'
    uint32_t unpackedSize;  // 0x02
    uint32_t reserved;      // 0x06
    uint32_t frameOffset;   // 0x0A
    uint32_t unknown;       // 0x0E
    uint32_t width;         // 0x12
    uint32_t height;        // 0x16
    uint16_t reserved2;     // 0x1A
    uint16_t type;          // 0x1C
};

struct BitmapFileHeader {
    char magic[2];
    uint32_t fileSize;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t dataOffset;
};

struct BitmapInfoHeader {
    uint32_t headerSize;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
};
#pragma pack(pop)

std::string getBaseName(const std::string& path) {
    fs::path fsPath(path);
    return fsPath.stem().string();
}

bool readPNG(const char* filename, std::vector<uint8_t>& imageData, uint32_t& width, uint32_t& height) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        std::cerr << "Cannot open PNG file" << std::endl;
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return false;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    // 转换为8位RGBA格式
    if (bit_depth == 16)
        png_set_strip_16(png);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    // 分配临时缓冲区
    std::vector<png_bytep> row_pointers(height);
    std::vector<uint8_t> tempData(height * width * 4);  // 临时存储区

    for (uint32_t y = 0; y < height; y++) {
        row_pointers[y] = &tempData[y * width * 4];
    }

    png_read_image(png, row_pointers.data());

    // 分配最终图像数据空间
    imageData.resize(height * width * 4);

    // 翻转图像并转换RGBA到BGRA
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t src_pixel = y * width * 4 + x * 4;
            uint32_t dst_pixel = (height - 1 - y) * width * 4 + x * 4;

            // 转换RGBA到BGRA并翻转
            imageData[dst_pixel + 0] = tempData[src_pixel + 2];  // B
            imageData[dst_pixel + 1] = tempData[src_pixel + 1];  // G
            imageData[dst_pixel + 2] = tempData[src_pixel + 0];  // R
            imageData[dst_pixel + 3] = tempData[src_pixel + 3];  // A
        }
    }

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return true;
}

bool convertToBM(const char* inputFile, const char* outputFile) {
    // 读取PNG文件
    std::vector<uint8_t> imageData;
    uint32_t width, height;
    if (!readPNG(inputFile, imageData, width, height)) {
        return false;
    }

    // 创建BM文件
    std::ofstream output(outputFile, std::ios::binary);
    if (!output) {
        std::cerr << "Cannot create output file" << std::endl;
        return false;
    }

    // 写入BM头部
    BMHeader bmh = { 0 };
    bmh.magic[0] = 'B';
    bmh.magic[1] = 'M';
    bmh.unpackedSize = width * height * 4;  // BGRA
    bmh.frameOffset = 0x46;  // 固定偏移量
    bmh.width = width;
    bmh.height = height;
    bmh.type = 0x20;  // 32位模式

    output.write(reinterpret_cast<char*>(&bmh), sizeof(bmh));

    // 填充剩余头部空间到frameOffset
    std::vector<uint8_t> padding(bmh.frameOffset - sizeof(bmh), 0);
    output.write(reinterpret_cast<char*>(padding.data()), padding.size());

    // 写入图像数据（与之前相同的处理逻辑）
    for (int y = height - 1; y >= 0; y--) {  // 从底部开始处理
        for (uint32_t x = 0; x < width; x++) {
            size_t pixel_pos = y * width * 4 + x * 4;
            uint8_t alpha = imageData[pixel_pos + 3];

            if (alpha == 0) {
                // 完全透明的像素：使用跳过指令
                uint8_t control = 0x00;
                uint8_t count = 3;
                output.write(reinterpret_cast<char*>(&control), 1);
                output.write(reinterpret_cast<char*>(&count), 1);
            }
            else if (alpha == 0xFF) {
                // 完全不透明的像素：使用FF指令
                uint8_t control = 0xFF;
                uint8_t count = 3;
                output.write(reinterpret_cast<char*>(&control), 1);
                output.write(reinterpret_cast<char*>(&count), 1);
                output.write(reinterpret_cast<char*>(&imageData[pixel_pos]), 3);
            }
            else {
                // 半透明的像素：使用alpha值作为控制字节
                uint8_t control = alpha;
                output.write(reinterpret_cast<char*>(&control), 1);

                // 写入BGR值（一次一个分量）
                for (int i = 0; i < 3; i++) {
                    uint8_t value = imageData[pixel_pos + i];
                    output.write(reinterpret_cast<char*>(&value), 1);

                    // 如果不是最后一个分量，需要重复控制字节
                    if (i < 2) {
                        output.write(reinterpret_cast<char*>(&control), 1);
                    }
                }
            }
        }
    }

    return true;
}

bool convertPNGtoBM(const fs::path& inputPath, const fs::path& outputPath) {
    try {
        // 确保输出目录存在
        fs::create_directories(outputPath.parent_path());

        if (!convertToBM(inputPath.string().c_str(), outputPath.string().c_str())) {
            std::cerr << "Failed to convert: " << inputPath << std::endl;
            return false;
        }
        std::cout << "Converted: " << inputPath << " -> " << outputPath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error processing " << inputPath << ": " << e.what() << std::endl;
        return false;
    }
}

// 处理整个文件夹
bool processDirectory(const std::string& inputDir, const std::string& outputDir) {
    try {
        // 检查输入目录是否存在
        if (!fs::exists(inputDir)) {
            std::cerr << "Input directory does not exist: " << inputDir << std::endl;
            return false;
        }

        // 创建输出目录
        fs::create_directories(outputDir);

        int successCount = 0;
        int totalCount = 0;

        // 遍历输入目录
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                // 转换为小写进行比较
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                if (extension == ".png") {
                    totalCount++;

                    // 构建输出文件路径
                    fs::path relativePath = fs::relative(entry.path(), inputDir);
                    fs::path outputPath = fs::path(outputDir) / relativePath.parent_path() /
                        (relativePath.stem().string() + ".abm");

                    if (convertPNGtoBM(entry.path(), outputPath)) {
                        successCount++;
                    }
                }
            }
        }

        std::cout << "\nConversion completed:" << std::endl;
        std::cout << "Total PNG files: " << totalCount << std::endl;
        std::cout << "Successfully converted: " << successCount << std::endl;
        std::cout << "Failed: " << (totalCount - successCount) << std::endl;

        return successCount > 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error processing directory: " << e.what() << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Made by julixian 2025.03.02" << std::endl;
        std::cout << "Usage: " << argv[0] << " <input_directory> <output_directory>" << std::endl;
        return 1;
    }

    if (processDirectory(argv[1], argv[2])) {
        std::cout << "\nDirectory processing completed successfully." << std::endl;
        return 0;
    }
    else {
        std::cerr << "\nDirectory processing failed." << std::endl;
        return 1;
    }
}