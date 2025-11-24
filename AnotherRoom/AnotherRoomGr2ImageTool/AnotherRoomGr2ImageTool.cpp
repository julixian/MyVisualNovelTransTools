#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <cstdint>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"

import std;
namespace fs = std::filesystem;

size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen) {
    // 环形缓冲区 (Window)
    uint8_t ring_buffer[4096] = { 0 };
    // 初始写入位置，对应汇编中的 v9 = 4078
    uint16_t ring_pos = 4078;

    unsigned int src_idx = 0;
    unsigned int dst_idx = 0;
    unsigned int flags = 0; // 控制位寄存器 v26

    while (src_idx < srclen && dst_idx < dstlen) {
        // 检查标志位是否耗尽 (v26 & 0x200 == 0)
        // 这里我们用标准逻辑：flags >>= 1，如果 flags < 0x100，则读取新字节
        flags >>= 1;
        if ((flags & 0x100) == 0) {
            if (src_idx >= srclen) break;
            uint8_t b = src[src_idx++];
            // 设置高位标记，这样移位8次后 flags 会小于 0x100
            flags = b | 0xFF00;
        }

        if (flags & 1) {
            // --- Literal Byte (直接复制) ---
            if (src_idx >= srclen) break;
            uint8_t b = src[src_idx++];

            if (dst_idx < dstlen) dst[dst_idx++] = b;

            // 更新环形缓冲区
            ring_buffer[ring_pos] = b;
            ring_pos = (ring_pos + 1) & 0xFFF;
        }
        else {
            // --- Reference (引用复制) ---
            if (src_idx + 1 >= srclen) break;

            uint8_t b1 = src[src_idx++];
            uint8_t b2 = src[src_idx++];

            // 逆向逻辑: v19 = (16 * (v17 & 0xF0)) | v15;
            // Offset = (Byte2的高4位 << 4) | Byte1
            uint16_t offset = ((b2 & 0xF0) << 4) | b1;

            // 逆向逻辑: v18 = v16 & 0xF; v21 = v18 + 2; loop <= v21
            // Length = (Byte2的低4位) + 3
            uint16_t length = (b2 & 0x0F) + 3;

            for (int i = 0; i < length; ++i) {
                uint16_t read_pos = (offset + i) & 0xFFF;
                uint8_t b = ring_buffer[read_pos];

                if (dst_idx < dstlen) dst[dst_idx++] = b;

                // 写入的数据也要放入环形缓冲区末尾
                ring_buffer[ring_pos] = b;
                ring_pos = (ring_pos + 1) & 0xFFF;
            }
        }
    }
    return dst_idx;
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

void grp2Png(const fs::path& inputPath, const fs::path& outputPath) {

    std::println("-------------------------------------");

    std::ifstream file(inputPath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::format("Failed to open file: {}", wide2Ascii(inputPath)));
    }
    else {
        std::println("Converting {} to PNG...", wide2Ascii(inputPath));
    }

    // 1. 读取第一文件头 (0x28 bytes)
    // 结构类似 BITMAPINFOHEADER
    std::vector<uint8_t> header1(0x28);
    file.read(reinterpret_cast<char*>(header1.data()), 0x28);
    if (file.gcount() != 0x28) throw std::runtime_error("File too small (header1).");

    // 获取宽高 (int32)
    int32_t width = *reinterpret_cast<int32_t*>(&header1[0x4]);
    int32_t height = *reinterpret_cast<int32_t*>(&header1[0x8]);

    if (width <= 0 || height <= 0) throw std::runtime_error("Invalid image dimensions.");

    std::println("Image Info: {}x{}", width, height);

    // 2. 跳过第二文件头 (0x24 bytes)
    file.seekg(0x24, std::ios::cur);

    // 3. 读取剩余的压缩数据
    std::vector<uint8_t> compressedData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (compressedData.empty()) throw std::runtime_error("No data found.");

    // 4. 准备解压缓冲区
    // GR2 是 24位色 (BGR)，每个像素 3 字节
    size_t pixelDataSize = width * height * 3;
    std::vector<uint8_t> decompressedData(pixelDataSize);

    // 5. 解压
    std::println("Decompressing {} bytes...", compressedData.size());
    size_t processed = lzss_decompress(decompressedData.data(), (unsigned int)pixelDataSize, compressedData.data(), (unsigned int)compressedData.size());

    if (processed != pixelDataSize) {
        std::println("Warning: Expected {} bytes, got {}. Image might be corrupted.", pixelDataSize, processed);
    }

    // 6. 像素处理 (BGR -> RGB) 和 垂直翻转 (Bottom-Up -> Top-Down)
    // Windows BMP 通常是倒序存储的 (Bottom-Up)，而 PNG 是正序 (Top-Down)。
    // 同时 GR2 存储的是 BGR，stb_image_write 需要 RGB。

    std::vector<uint8_t> finalPixels(pixelDataSize);
    int stride = width * 3;

    for (int y = 0; y < height; ++y) {
        // 源行索引 (倒序读取：从最后一行开始读)
        // 如果图片出来是倒的，把 srcY 改成 y 即可
        int srcY = height - 1 - y;

        // 目标行索引 (正序写入)
        int dstY = y;

        uint8_t* pSrcRow = &decompressedData[srcY * stride];
        uint8_t* pDstRow = &finalPixels[dstY * stride];

        for (int x = 0; x < width; ++x) {
            uint8_t b = pSrcRow[x * 3 + 0];
            uint8_t g = pSrcRow[x * 3 + 1];
            uint8_t r = pSrcRow[x * 3 + 2];

            // 写入 RGB
            pDstRow[x * 3 + 0] = r;
            pDstRow[x * 3 + 1] = g;
            pDstRow[x * 3 + 2] = b;
        }
    }

    // 7. 写入 PNG
    if (stbi_write_png(wide2Ascii(outputPath.wstring()).c_str(), width, height, 3, finalPixels.data(), width * 3)) {
        std::println("Converting complete. Output saved to {}", wide2Ascii(outputPath));
    }
    else {
        throw std::runtime_error(std::format("Failed to write PNG: {}", wide2Ascii(outputPath)));
    }
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.24\n"
        "Usage:\n"
        " {0} <input_gr2> <output_png>\n"
        " {0} <input_gr2_folder> <output_folder>\n"
        , wide2Ascii(programPath.filename()));
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    try {

        if (argc < 3) {
            printUsage(argv[0]);
            return 1;
        }

        const fs::path inputPath(argv[1]);
        const fs::path outputPath(argv[2]);

        if (fs::is_directory(inputPath)) {
            for (const auto& entry : fs::recursive_directory_iterator(inputPath)) {
                if (!entry.is_regular_file() || _wcsicmp(entry.path().extension().c_str(), L".gr2") != 0) {
                    continue;
                }
                const fs::path outputFilePath = outputPath / fs::relative(entry.path(), inputPath).replace_extension(L".png");
                if (!fs::exists(outputFilePath.parent_path())) {
                    fs::create_directories(outputFilePath.parent_path());
                }
                grp2Png(entry.path(), outputFilePath);
            }
        }
        else if (fs::is_regular_file(inputPath)) {
            if (outputPath.has_parent_path() && !fs::exists(outputPath.parent_path())) {
                fs::create_directories(outputPath.parent_path());
            }
            grp2Png(inputPath, outputPath);
        }
        else {
            throw std::runtime_error("Invalid input path.");
        }

    }
    catch (const std::exception& e) {
        std::println("Error: {}", e.what());
        return 1;
    }

    return 0;
}
