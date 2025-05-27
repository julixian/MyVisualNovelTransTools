#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <png.h>

// 常量定义
constexpr int GR2_VERSION = 2;
constexpr int BPP = 16;
constexpr int COUNT_BITS = 5;
constexpr int COUNT_MASK = 0x1F;
constexpr int WINDOW_SIZE = 2048;
constexpr int MAX_MATCH = 32;
constexpr int MIN_MATCH = 3;

// 写入小端序16位值
void write_uint16_le(std::ofstream& file, uint16_t value) {
    char buffer[2];
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    file.write(buffer, 2);
}

// 写入小端序32位值
void write_uint32_le(std::ofstream& file, uint32_t value) {
    char buffer[4];
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
    file.write(buffer, 4);
}

class PNG2GR2Converter {
private:
    std::string input_file;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> raw_data;
    std::vector<uint8_t> compressed_data;
    std::vector<bool> control_bits;

    // 读取PNG图像
    bool load_png() {
        FILE* fp = fopen(input_file.c_str(), "rb");
        if (!fp) {
            std::cerr << "无法打开文件: " << input_file << std::endl;
            return false;
        }

        // 检查PNG签名
        png_byte header[8];
        if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
            std::cerr << "不是有效的PNG文件" << std::endl;
            fclose(fp);
            return false;
        }

        // 初始化PNG读取结构
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png_ptr) {
            std::cerr << "无法创建PNG读取结构" << std::endl;
            fclose(fp);
            return false;
        }

        // 初始化PNG信息结构
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            std::cerr << "无法创建PNG信息结构" << std::endl;
            png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            fclose(fp);
            return false;
        }

        // 设置错误处理
        if (setjmp(png_jmpbuf(png_ptr))) {
            std::cerr << "PNG读取错误" << std::endl;
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            fclose(fp);
            return false;
        }

        // 初始化PNG I/O
        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);

        // 读取PNG信息
        png_read_info(png_ptr, info_ptr);

        // 获取图像信息
        width = png_get_image_width(png_ptr, info_ptr);
        height = png_get_image_height(png_ptr, info_ptr);
        png_byte color_type = png_get_color_type(png_ptr, info_ptr);
        png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        // 转换为RGB格式
        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png_ptr);
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png_ptr);
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png_ptr);
        if (bit_depth == 16)
            png_set_strip_16(png_ptr);
        if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png_ptr);
        if (color_type & PNG_COLOR_MASK_ALPHA)
            png_set_strip_alpha(png_ptr);

        // 更新信息
        png_read_update_info(png_ptr, info_ptr);

        // 获取更新后的行字节数
        png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);

        // 分配内存存储图像数据
        png_bytep* row_pointers = new png_bytep[height];
        for (int y = 0; y < height; y++) {
            row_pointers[y] = new png_byte[rowbytes];
        }

        // 读取图像数据
        png_read_image(png_ptr, row_pointers);

        // 转换为RGB565格式
        raw_data.resize(width * height * 2); // 16位 = 2字节/像素

        for (int y = 0; y < height; y++) {
            png_bytep row = row_pointers[y];
            for (int x = 0; x < width; x++) {
                // 确保我们处理的是RGB格式
                uint8_t r = row[x * 3];     // 红色分量
                uint8_t g = row[x * 3 + 1]; // 绿色分量
                uint8_t b = row[x * 3 + 2]; // 蓝色分量

                // 转换为RGB565格式 (5位R, 6位G, 5位B)
                uint8_t r5 = (r >> 3) & 0x1F;
                uint8_t g6 = (g >> 2) & 0x3F;
                uint8_t b5 = (b >> 3) & 0x1F;

                // 组合为16位值 (RGB565)
                uint16_t rgb565 = (r5 << 11) | (g6 << 5) | b5;

                // 存储为小端序
                int idx = (y * width + x) * 2;
                raw_data[idx] = rgb565 & 0xFF;
                raw_data[idx + 1] = (rgb565 >> 8) & 0xFF;
            }
        }

        // 清理
        for (int y = 0; y < height; y++) {
            delete[] row_pointers[y];
        }
        delete[] row_pointers;

        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(fp);

        std::cout << "图像信息: " << width << "x" << height << ", 将转换为16位RGB565" << std::endl;
        return true;
    }

    // 压缩数据
    bool compress_lz77() {
        if (raw_data.empty()) {
            std::cerr << "没有原始数据可压缩" << std::endl;
            return false;
        }

        compressed_data.clear();
        control_bits.clear();

        size_t pos = 0;
        size_t data_len = raw_data.size();

        while (pos < data_len) {
            // 查找最佳匹配
            int best_length = 0;
            int best_offset = 0;

            // 限制回溯窗口大小为2048
            size_t search_start = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;

            // 尝试找到最长匹配
            if (pos < data_len - MIN_MATCH + 1) {
                for (size_t offset = 1; offset <= pos - search_start; offset++) {
                    // 计算可能的匹配长度
                    size_t match_pos = pos - offset;
                    int match_len = 0;

                    while (pos + match_len < data_len &&
                        match_len < MAX_MATCH &&
                        raw_data[match_pos + match_len] == raw_data[pos + match_len]) {
                        match_len++;
                    }

                    // 如果找到更好的匹配
                    if (match_len >= MIN_MATCH && match_len > best_length) {
                        best_length = match_len;
                        best_offset = offset;
                    }
                }
            }

            // 输出匹配或字面量
            if (best_length >= MIN_MATCH) {
                // 确保长度不超过可表示的最大值
                if (best_length > COUNT_MASK + 1) {
                    best_length = COUNT_MASK + 1;
                }

                // 编码为引用 (5位长度，11位偏移)
                uint16_t offset_length = ((best_offset - 1) << COUNT_BITS) | ((best_length - 1) & COUNT_MASK);

                // 存储为小端序
                compressed_data.push_back(offset_length & 0xFF);
                compressed_data.push_back((offset_length >> 8) & 0xFF);

                // 添加控制位 1 (引用)
                control_bits.push_back(true);

                pos += best_length;
            }
            else {
                // 直接输出字节
                compressed_data.push_back(raw_data[pos]);

                // 添加控制位 0 (字面量)
                control_bits.push_back(false);

                pos++;
            }
        }

        return true;
    }

    // 创建GR2文件
    bool create_gr2_file(const std::string& output_file) {
        if (compressed_data.empty() || control_bits.empty()) {
            std::cerr << "没有压缩数据" << std::endl;
            return false;
        }

        std::ofstream file(output_file, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "无法创建输出文件: " << output_file << std::endl;
            return false;
        }

        // 写入文件头
        file.write("GR", 2);                   // 标识符
        char version = '0' + GR2_VERSION;
        file.write(&version, 1);               // 版本号
        write_uint16_le(file, BPP);            // 色深
        write_uint32_le(file, width);          // 宽度
        write_uint32_le(file, height);         // 高度
        write_uint32_le(file, raw_data.size());// 解压后大小

        // 写入控制位流长度 (位数)
        write_uint32_le(file, control_bits.size());

        // 写入控制位流数据
        std::vector<uint8_t> control_bytes((control_bits.size() + 7) / 8, 0);
        for (size_t i = 0; i < control_bits.size(); i++) {
            if (control_bits[i]) {
                size_t byte_idx = i / 8;
                size_t bit_idx = i % 8;
                control_bytes[byte_idx] |= (1 << bit_idx);
            }
        }
        file.write(reinterpret_cast<char*>(control_bytes.data()), control_bytes.size());

        // 写入压缩流大小
        write_uint32_le(file, compressed_data.size());

        // 写入压缩数据
        file.write(reinterpret_cast<char*>(compressed_data.data()), compressed_data.size());

        file.close();

        std::cout << "GR2文件已创建: " << output_file << std::endl;
        std::cout << "原始大小: " << raw_data.size() << " 字节" << std::endl;
        size_t total_compressed_size = compressed_data.size() + control_bytes.size() + 4;
        std::cout << "压缩后大小: " << total_compressed_size << " 字节" << std::endl;
        double compression_ratio = 100.0 * (1.0 - static_cast<double>(total_compressed_size) / raw_data.size());
        std::cout << "压缩率: " << compression_ratio << "%" << std::endl;

        return true;
    }

public:
    PNG2GR2Converter(const std::string& input) : input_file(input) {}

    bool convert(const std::string& output_file) {
        try {
            // 加载PNG图像
            if (!load_png()) {
                return false;
            }

            // 压缩数据
            if (!compress_lz77()) {
                return false;
            }

            // 创建GR2文件
            if (!create_gr2_file(output_file)) {
                return false;
            }

            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "转换失败: " << e.what() << std::endl;
            return false;
        }
    }
};

// 获取文件扩展名
std::string get_file_extension(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(pos);
    }
    return "";
}

// 获取不带扩展名的文件名
std::string get_file_basename(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(0, pos);
    }
    return filename;
}

void print_usage(const char* program_name) {
    std::cout << "用法: " << program_name << " <输入PNG文件> [输出GR2文件]" << std::endl;
    std::cout << "如果未指定输出文件，将使用输入文件名并添加.grp扩展名" << std::endl;
}

int main(int argc, char* argv[]) {
    // 检查参数
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file;

    // 确定输出文件名
    if (argc == 3) {
        output_file = argv[2];
    }
    else {
        output_file = get_file_basename(input_file) + ".grp";
    }

    // 检查输入文件扩展名
    std::string ext = get_file_extension(input_file);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png") {
        std::cerr << "警告: 输入文件可能不是PNG文件 (" << ext << ")" << std::endl;
    }

    // 执行转换
    PNG2GR2Converter converter(input_file);
    if (!converter.convert(output_file)) {
        std::cerr << "转换失败" << std::endl;
        return 1;
    }

    std::cout << "转换完成" << std::endl;
    return 0;
}