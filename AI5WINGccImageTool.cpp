#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <png.h>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct GccHeader {
    uint32_t signature;  // 'G24m'
    int16_t offsetX;
    int16_t offsetY;
    uint16_t width;
    uint16_t height;
    uint32_t alphaOffset;
    uint32_t imageOffset;
    uint32_t reserved;
    uint16_t alphaWidth;
    uint16_t alphaHeight;
    uint32_t alphaInternalOffset;
};
#pragma pack(pop)

class LzssCompressor {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        for (size_t i = 0; i < input.size(); i += 8) {
            output.push_back(0xFF);
            for (size_t j = 0; j < 8 && i + j < input.size(); ++j) {
                output.push_back(input[i + j]);
            }
        }
        return output;
    }
};

class AlphaCompressor {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> controlBytes;
        std::vector<uint8_t> dataBytes;

        // 每8个像素一组，生成一个控制字节
        for (size_t i = 0; i < input.size(); i += 8) {
            uint8_t controlByte = 0;

            // 处理这8个像素
            for (size_t j = 0; j < 8 && (i + j) < input.size(); ++j) {
                // 设置控制位为0（直接字节）
                controlByte |= (0 << j);
                // 添加数据字节
                dataBytes.push_back(input[i + j]);
            }

            // 添加控制字节
            controlBytes.push_back(controlByte);
        }

        // 合并输出：先是所有控制字节，然后是所有数据字节
        std::vector<uint8_t> output;
        output.insert(output.end(), controlBytes.begin(), controlBytes.end());
        output.insert(output.end(), dataBytes.begin(), dataBytes.end());

        return output;
    }
};

// PNG读取结构体
struct PngData {
    std::vector<uint8_t> rgba;
    uint32_t width;
    uint32_t height;
};

// 读取PNG文件
PngData readPng(const std::string& filename) {
    PngData result;
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) throw std::runtime_error("Cannot open PNG file");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) throw std::runtime_error("Cannot create PNG read struct");

    png_infop info = png_create_info_struct(png);
    if (!info) throw std::runtime_error("Cannot create PNG info struct");

    if (setjmp(png_jmpbuf(png))) throw std::runtime_error("PNG error");

    png_init_io(png, fp);
    png_read_info(png, info);

    result.width = png_get_image_width(png, info);
    result.height = png_get_image_height(png, info);

    png_set_expand(png);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);
    png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(png, info);

    result.rgba.resize(result.width * result.height * 4);
    std::vector<png_bytep> row_pointers(result.height);
    for (size_t y = 0; y < result.height; ++y) {
        // 修改这里：从下到上读取PNG数据
        row_pointers[result.height - 1 - y] = result.rgba.data() + y * result.width * 4;
    }
    png_read_image(png, row_pointers.data());

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return result;
}

// 读取原始GCC文件头
GccHeader readGccHeader(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open GCC file");

    GccHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    return header;
}

bool processFiles(const std::string& pngDir, const std::string& origDir, const std::string& outDir) {

    try {
        fs::create_directories(outDir);

        for (const auto& entry : fs::directory_iterator(pngDir)) {
            if (entry.path().extension() != ".png") {
                continue;
            }

            std::string baseName = entry.path().stem().string();
            fs::path origGccPath = fs::path(origDir) / (baseName + ".gcc");
            fs::path outGccPath = fs::path(outDir) / (baseName + ".gcc");

            // 读取PNG文件以获取尺寸
            PngData pngData = readPng(entry.path().string());

            // 创建默认的GCC头部
            GccHeader newHeader = { 0 };
            newHeader.signature = 0x6D343247; // G24m
            newHeader.width = pngData.width;
            newHeader.height = pngData.height;
            newHeader.imageOffset = 0x20;

            if (fs::exists(origGccPath)) {
                // 如果存在原始GCC文件，读取它的头部
                std::ifstream origFile(origGccPath, std::ios::binary);
                GccHeader origHeader;
                origFile.read(reinterpret_cast<char*>(&origHeader), sizeof(origHeader));
                newHeader.imageOffset = origHeader.imageOffset;

                if (origHeader.signature == 0x6d343247 || origHeader.signature == 0x6d343252) {
                    // 使用原始文件的Alpha尺寸和偏移
                    newHeader.offsetX = origHeader.offsetX;
                    newHeader.offsetY = origHeader.offsetY;
                    newHeader.alphaWidth = origHeader.alphaWidth;
                    newHeader.alphaHeight = origHeader.alphaHeight;
                }
                else if (origHeader.signature == 0x6e343247 || origHeader.signature == 0x6e343252) {
                    // 24n模式使用PNG尺寸作为Alpha尺寸
                    newHeader.offsetX = origHeader.offsetX;
                    newHeader.offsetY = origHeader.offsetY;
                    newHeader.alphaWidth = pngData.width;
                    newHeader.alphaHeight = pngData.height;
                }

            }
            else {
                newHeader.offsetX = 0;
                newHeader.offsetY = 0;
                newHeader.alphaWidth = pngData.width;
                newHeader.alphaHeight = pngData.height;
            }

            // 处理图像数据
            std::vector<uint8_t> bgrData;
            std::vector<uint8_t> alphaData(newHeader.alphaWidth * newHeader.alphaHeight, 0);

            // 转换RGBA到BGR并提取Alpha
            bgrData.reserve(pngData.width * pngData.height * 3);
            for (size_t y = 0; y < pngData.height; ++y) {
                for (size_t x = 0; x < pngData.width; ++x) {
                    size_t srcPos = (y * pngData.width + x) * 4;
                    bgrData.push_back(pngData.rgba[srcPos + 2]); // B
                    bgrData.push_back(pngData.rgba[srcPos + 1]); // G
                    bgrData.push_back(pngData.rgba[srcPos + 0]); // R

                    // 放置Alpha数据
                    size_t alphaPos = ((newHeader.alphaHeight - newHeader.offsetY - pngData.height + y) *
                        newHeader.alphaWidth + x + newHeader.offsetX);
                    alphaData[alphaPos] = pngData.rgba[srcPos + 3];
                }
            }

            // 压缩数据
            LzssCompressor lzssComp;
            AlphaCompressor alphaComp;
            auto compressedBgr = lzssComp.compress(bgrData);
            auto compressedAlpha = alphaComp.compress(alphaData);

            // 更新头部信息
            newHeader.alphaOffset = compressedBgr.size();
            newHeader.alphaInternalOffset = (alphaData.size() + 7) / 8;

            // 写入文件
            std::ofstream outFile(outGccPath, std::ios::binary);
            if (!outFile) {
                std::cerr << "Cannot create output file: " << outGccPath << std::endl;
                continue;
            }

            outFile.write(reinterpret_cast<char*>(&newHeader), sizeof(newHeader));
            outFile.write(reinterpret_cast<char*>(compressedBgr.data()), compressedBgr.size());
            outFile.write(reinterpret_cast<char*>(compressedAlpha.data()), compressedAlpha.size());

            std::cout << "Successfully processed " << baseName << std::endl;
        }

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.01.22" << std::endl;
        std::cout << "Usage: " << argv[0]
            << " <png_directory> <original_gcc_directory> <output_directory>"
            << std::endl;
        return 1;
    }

    if (!processFiles(argv[1], argv[2], argv[3])) {
        std::cerr << "Processing failed" << std::endl;
        return 1;
    }

    std::cout << "All files processed successfully" << std::endl;
    return 0;
}
