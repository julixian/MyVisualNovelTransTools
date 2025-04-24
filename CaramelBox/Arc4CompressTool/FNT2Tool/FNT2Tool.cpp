#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <zlib.h>
#include <map>
#include <set>
#include <filesystem>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <locale>
#include <codecvt>
#include <cstring>

namespace fs = std::filesystem;

#pragma pack(1)
struct FNTHDR
{
    char sig[4] = { 'F','N','T','2'};
    uint32_t unk = 0;
    uint32_t height;
    uint32_t comprFlag = 1;
};
#pragma pack()

std::vector<uint8_t> zlibCompress(const std::vector<uint8_t>& input, int compressionLevel = Z_DEFAULT_COMPRESSION) {
    // 检查输入参数
    if (input.empty()) {
        return std::vector<uint8_t>();
    }

    if ((compressionLevel < Z_NO_COMPRESSION || compressionLevel > Z_BEST_COMPRESSION) && compressionLevel != -1) {
        std::cout << "压缩级别必须在0-9之间" << std::endl;
    }

    // 初始化zlib流
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    // 初始化压缩流
    if (deflateInit(&zs, compressionLevel) != Z_OK) {
        throw std::runtime_error("无法初始化zlib压缩");
    }

    // 设置输入数据
    zs.next_in = const_cast<Bytef*>(input.data());
    zs.avail_in = static_cast<uInt>(input.size());

    // 预估压缩后的大小（通常压缩后的数据不会超过原始数据的1.1倍加上12字节）
    size_t outputSize = input.size() * 1.1 + 12;
    std::vector<uint8_t> output(outputSize);

    // 设置输出缓冲区
    zs.next_out = output.data();
    zs.avail_out = static_cast<uInt>(output.size());

    // 执行压缩
    int result = deflate(&zs, Z_FINISH);

    // 释放资源
    deflateEnd(&zs);

    if (result != Z_STREAM_END) {
        throw std::runtime_error("zlib压缩失败: " + std::to_string(result));
    }

    // 调整输出vector大小为实际压缩后的大小
    output.resize(zs.total_out);

    return output;
}

// 检查CP932编码是否有效
bool isValidCP932Code(uint16_t code) {
    // 单字节字符 (ASCII 和半角片假名)
    if (code <= 0xFF) {
        return (code >= 0x20 && code <= 0x7E) || (code >= 0xA1 && code <= 0xDF);
    }

    // 获取双字节的两个部分
    uint8_t first = (code >> 8) & 0xFF;
    uint8_t second = code & 0xFF;

    // 第一个字节的有效范围
    bool validFirst = (first >= 0x81 && first <= 0x9F) || (first >= 0xE0 && first <= 0xFC);

    // 第二个字节的有效范围
    bool validSecond = (second >= 0x40 && second <= 0x7E) || (second >= 0x80 && second <= 0xFC);

    return validFirst && validSecond;
}

// CP932编码转Unicode
uint32_t cp932ToUnicode(uint16_t cp932Code) {
    // 使用转换表或系统API
    // 这里使用一个简单的方法：创建一个包含CP932字符的字符串，然后转换为宽字符串

    std::string input;
    if (cp932Code <= 0xFF) {
        // 单字节字符
        input.push_back(static_cast<char>(cp932Code));
    }
    else {
        // 双字节字符
        input.push_back(static_cast<char>((cp932Code >> 8) & 0xFF));
        input.push_back(static_cast<char>(cp932Code & 0xFF));
    }

    // 设置本地化环境以支持CP932
    try {
        // Windows平台
#ifdef _WIN32
        static std::wstring_convert<std::codecvt_byname<wchar_t, char, std::mbstate_t>>
            converter(new std::codecvt_byname<wchar_t, char, std::mbstate_t>(".932"));
        std::wstring wide = converter.from_bytes(input);
#else
// Linux/macOS平台
        setlocale(LC_ALL, "ja_JP.SJIS");
        std::mbstate_t state = std::mbstate_t();
        wchar_t wc[2] = { 0 };
        const char* src = input.c_str();
        size_t res = std::mbrtowc(wc, src, input.size(), &state);
        std::wstring wide;
        if (res != static_cast<size_t>(-1) && res != static_cast<size_t>(-2)) {
            wide = wc;
        }
#endif

        if (!wide.empty()) {
            return static_cast<uint32_t>(wide[0]);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Conversion error for CP932 code 0x" << std::hex << cp932Code
            << ": " << e.what() << std::endl;
    }

    return 0; // 转换失败
}

// 生成指定字符的灰度数据矩阵
std::vector<uint8_t> generateGrayscaleMatrix(FT_Face face, uint32_t unicode, int width, int height) {
    // 设置字体大小 (以像素为单位)
    if (FT_Set_Pixel_Sizes(face, 0, height)) {
        std::cerr << "Error: Could not set font size" << std::endl;
        return {};
    }

    // 加载字符
    FT_UInt glyph_index = FT_Get_Char_Index(face, unicode);
    if (glyph_index == 0) {
        // 字体中没有这个字符
        return {};
    }

    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) {
        std::cerr << "Error: Could not load glyph for Unicode: " << std::hex << unicode << std::endl;
        return {};
    }

    // 渲染为灰度位图
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
        std::cerr << "Error: Could not render glyph for Unicode: " << std::hex << unicode << std::endl;
        return {};
    }

    // 创建输出矩阵并初始化为0
    std::vector<uint8_t> matrix(width * height, 0);

    // 获取字形位图
    FT_Bitmap bitmap = face->glyph->bitmap;

    // 判断是否为标点符号
    bool isPunctuation = false;

    // 日文/中文标点符号的Unicode范围
    if ((unicode >= 0x3000 && unicode <= 0x303F) ||  // CJK符号和标点
        (unicode >= 0xFF00 && unicode <= 0xFF0F) ||  // 全角ASCII标点
        (unicode >= 0xFF1A && unicode <= 0xFF20) ||  // 全角ASCII标点
        (unicode >= 0xFF3B && unicode <= 0xFF40) ||  // 全角ASCII标点
        (unicode >= 0xFF5B && unicode <= 0xFF65) ||  // 全角ASCII标点
        (unicode >= 0x2000 && unicode <= 0x206F) ||  // 一般标点
        // 添加其他特定标点符号
        unicode == 0x3002 ||  // 句号
        unicode == 0xFF0E ||  // 全角句号
        unicode == 0x3001 ||  // 顿号
        unicode == 0xFF0C ||  // 全角逗号
        unicode == 0xFF1F ||  // 全角问号
        unicode == 0xFF01) {  // 全角感叹号
        isPunctuation = true;
    }

    int start_x, start_y;

    if (isPunctuation) {
        // 对于标点符号，使用字形的原始位置信息
        // 但需要考虑上下翻转

        // 设置基线位置
        int baseline_x = 0;
        int baseline_y = height * 3 / 4;  // 假设基线在高度的3/4处

        // 计算在翻转前的位置
        int orig_start_x = baseline_x + face->glyph->bitmap_left;
        int orig_start_y = baseline_y - face->glyph->bitmap_top;

        // 应用翻转：保持x不变，计算翻转后的y位置
        // 我们希望标点符号在左下角，所以在翻转后应该在左下角
        start_x = orig_start_x;
        start_y = height - bitmap.rows - orig_start_y;  // 这样会保持在下方

        int punctuation_down_offset = height / 6;  // 向下偏移1/6的高度
        start_y -= punctuation_down_offset;

        // 确保标点符号不会超出边界
        if (start_x < 0) start_x = 0;
        if (start_y < 0) start_y = 0;
        if (start_x + bitmap.width > width) start_x = width - bitmap.width;
        if (start_y + bitmap.rows > height) start_y = height - bitmap.rows;
    }
    else {
        // 对于非标点符号，居中显示
        start_x = (width - bitmap.width) / 2;
        start_y = (height - bitmap.rows) / 2;
    }

    // 复制位图数据到矩阵中（上下翻转）
    for (unsigned int y = 0; y < bitmap.rows; y++) {
        // 翻转读取的源位图数据
        int src_y = bitmap.rows - 1 - y;

        for (unsigned int x = 0; x < bitmap.width; x++) {
            int pos_x = start_x + x;
            int pos_y = start_y + y;

            // 确保在边界内
            if (pos_x >= 0 && pos_x < width && pos_y >= 0 && pos_y < height) {
                // 获取位图中的灰度值（从翻转的位置读取）
                uint8_t value = bitmap.buffer[src_y * bitmap.pitch + x];
                matrix[pos_y * width + pos_x] = value;
            }
        }
    }

    return matrix;
}

// 将灰度矩阵保存为二进制文件
void saveMatrixToFile(const std::vector<uint8_t>& matrix, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return;
    }

    file.write(reinterpret_cast<const char*>(matrix.data()), matrix.size());
    file.close();
}

// 获取CP932所有可能的编码
std::vector<uint16_t> getAllCP932Codes() {
    std::vector<uint16_t> codes;

    // 添加单字节字符 (ASCII 和半角片假名)
    for (uint16_t code = 0x20; code <= 0x7E; code++) {
        codes.push_back(code);
    }
    for (uint16_t code = 0xA1; code <= 0xDF; code++) {
        codes.push_back(code);
    }

    // 添加双字节字符
    // 第一区 (0x8140 - 0x9FFC)
    for (uint16_t first = 0x81; first <= 0x9F; first++) {
        for (uint16_t second = 0x40; second <= 0xFC; second++) {
            if (second >= 0x40 && second <= 0x7E) {
                codes.push_back((first << 8) | second);
            }
            else if (second >= 0x80 && second <= 0xFC) {
                codes.push_back((first << 8) | second);
            }
        }
    }

    // 第二区 (0xE040 - 0xFCFC)
    for (uint16_t first = 0xE0; first <= 0xFC; first++) {
        for (uint16_t second = 0x40; second <= 0xFC; second++) {
            if (second >= 0x40 && second <= 0x7E) {
                codes.push_back((first << 8) | second);
            }
            else if (second >= 0x80 && second <= 0xFC) {
                codes.push_back((first << 8) | second);
            }
        }
    }

    return codes;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Made by julixian 2025.04.24\n"
            << "Usage: " << argv[0] << " <ttf/otf_font_path> <output_path> <height>" << std::endl;
        return 1;
    }

    std::string fontPath = argv[1];
    std::string oFontPath = argv[2];
    // 矩阵大小
    int width = std::stoi(argv[3]);
    int height = width;
    std::ofstream ofs(oFontPath, std::ios::binary);

    // 初始化 FreeType 库
    FT_Library library;
    if (FT_Init_FreeType(&library)) {
        std::cerr << "Error: Could not initialize FreeType library" << std::endl;
        return 1;
    }

    // 加载字体
    FT_Face face;
    if (FT_New_Face(library, fontPath.c_str(), 0, &face)) {
        std::cerr << "Error: Could not load font file: " << fontPath << std::endl;
        FT_Done_FreeType(library);
        return 1;
    }


    FNTHDR hdr;
    hdr.height = height;
    ofs.write((char*)&hdr, sizeof(hdr));

    // 获取所有可能的CP932编码
    std::vector<uint16_t> cp932Codes = getAllCP932Codes();

    std::cout << "Total CP932 codes to process: " << cp932Codes.size() << std::endl;

    // 统计成功处理的字符数
    int processedCount = 0;
    int failedCount = 0;
    uint32_t currentOffset = 0x60010;

    for (uint16_t code : cp932Codes) {
        // 转换为Unicode
        uint32_t unicode = cp932ToUnicode(code);
        if (unicode == 0) {
            uint32_t zero = 0;
            ofs.seekp(code * 4 + 16);
            ofs.write((char*)&zero, 4);
            ofs.seekp(16 + 0x10000 * 4 + code * 2);
            ofs.write((char*)&zero, 2);
            failedCount++;
            continue;
        }

        // 生成灰度矩阵
        std::vector<uint8_t> matrix;

        if (code < 0x8140) {
            matrix = generateGrayscaleMatrix(face, unicode, width / 2, height);
        }
        else {
            matrix = generateGrayscaleMatrix(face, unicode, width, height);
        }

        if (!matrix.empty()) {
            std::vector<uint8_t> compressedMatrix = zlibCompress(matrix);
            uint16_t comprlen = compressedMatrix.size();
            ofs.seekp(code * 4 + 16);
            ofs.write((char*)&currentOffset, 4);
            ofs.seekp(16 + 0x10000 * 4 + code * 2);
            ofs.write((char*)&comprlen, 2);
            ofs.seekp(currentOffset);
            ofs.write((char*)compressedMatrix.data(), compressedMatrix.size());
            currentOffset += compressedMatrix.size();

            processedCount++;
            if (processedCount % 100 == 0) {
                std::cout << "Processed " << processedCount << " characters..." << std::endl;
            }
        }
        else {
            uint32_t zero = 0;
            ofs.seekp(code * 4 + 16);
            ofs.write((char*)&zero, 4);
            ofs.seekp(16 + 0x10000 * 4 + code * 2);
            ofs.write((char*)&zero, 2);
            failedCount++;
        }
    }

    ofs.close();
    // 清理资源
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    std::cout << "Completed! Successfully processed " << processedCount << " characters." << std::endl;
    std::cout << "Failed to process " << failedCount << " characters." << std::endl;

    return 0;
}
