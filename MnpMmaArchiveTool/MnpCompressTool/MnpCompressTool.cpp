#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm> // for std::max

// 向右循环移位，是解压时 rotate_left 的逆操作
inline uint8_t rotate_right(uint8_t value, int shift) {
    return value;
}

// 格式常量
const int MIN_MATCH_LENGTH = 3;
const int MAX_MATCH_LENGTH = 34; // (0x1F) + 3
const int MAX_OFFSET = 2048;     // 11 bits for offset -> 2^11

/**
 * @brief 将原始数据压缩为 MMA/MNP 格式的 LZ 压缩流
 * @param raw_data 待压缩的原始数据
 * @param compressed_data 用于存放压缩后数据的字节向量
 * @return 如果压缩成功返回 true
 */
bool compress_lz(const std::vector<uint8_t>& raw_data, std::vector<uint8_t>& compressed_data) {
    compressed_data.clear();
    compressed_data.push_back(0xC0); // 写入魔术字节

    size_t input_pos = 0;

    while (input_pos < raw_data.size()) {
        uint8_t control_byte = 0;
        std::vector<uint8_t> chunk_data;

        // 记住控制字节在输出流中的位置，我们稍后会回来更新它
        size_t control_byte_pos = compressed_data.size();
        compressed_data.push_back(0); // 先放一个占位符

        // 处理一个最多包含8个操作的块
        for (int bit = 0; bit < 8; ++bit) {
            if (input_pos >= raw_data.size()) break;

            // --- 寻找最佳匹配 ---
            int best_match_length = 0;
            int best_match_offset = 0;

            // 定义搜索窗口
            size_t search_start = (input_pos > MAX_OFFSET) ? (input_pos - MAX_OFFSET) : 0;

            // 限制最大可匹配长度
            size_t max_possible_length = std::min((size_t)MAX_MATCH_LENGTH, raw_data.size() - input_pos);

            // 从近到远搜索，这样可以优先找到偏移量小的匹配
            for (size_t p = input_pos - 1; p >= search_start && p != (size_t)-1; --p) {
                int current_match_length = 0;
                while (current_match_length < max_possible_length &&
                    raw_data[p + current_match_length] == raw_data[input_pos + current_match_length]) {
                    current_match_length++;
                }

                if (current_match_length > best_match_length) {
                    best_match_length = current_match_length;
                    best_match_offset = input_pos - p;
                }
            }

            // --- 决策：使用引用还是字面量 ---
            if (best_match_length >= MIN_MATCH_LENGTH) {
                // 使用引用
                control_byte |= (1 << (7 - bit)); // 设置控制位为 1

                uint16_t encoded_offset = best_match_offset - 1;
                uint16_t encoded_length = best_match_length - 3;

                uint16_t packed_word = (encoded_offset << 5) | encoded_length;

                chunk_data.push_back(packed_word >> 8);      // 高位字节
                chunk_data.push_back(packed_word & 0xFF);  // 低位字节

                input_pos += best_match_length;
            }
            else {
                // 使用字面量
                // 控制位默认为 0，无需操作 control_byte

                uint8_t rotated_byte = rotate_right(raw_data[input_pos], 5);
                chunk_data.push_back(rotated_byte);

                input_pos++;
            }
        }

        // 回填正确的控制字节
        compressed_data[control_byte_pos] = control_byte;
        // 追加这个块的数据
        compressed_data.insert(compressed_data.end(), chunk_data.begin(), chunk_data.end());
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "用法: " << argv[0] << " <输入原始文件> <输出压缩文件>" << std::endl;
        return 1;
    }

    std::string input_filename = argv[1];
    std::string output_filename = argv[2];

    // 1. 读取输入文件
    std::ifstream input_file(input_filename, std::ios::binary);
    if (!input_file) {
        std::cerr << "错误：无法打开输入文件 '" << input_filename << "'" << std::endl;
        return 1;
    }
    std::vector<uint8_t> raw_data(
        (std::istreambuf_iterator<char>(input_file)),
        std::istreambuf_iterator<char>()
    );
    input_file.close();
    std::cout << "读取了 " << raw_data.size() << " 字节的原始数据。" << std::endl;

    // 2. 执行压缩
    std::vector<uint8_t> compressed_data;
    if (!compress_lz(raw_data, compressed_data)) {
        std::cerr << "压缩失败！" << std::endl;
        return 1;
    }
    std::cout << "压缩成功！压缩后大小为 " << compressed_data.size() << " 字节。" << std::endl;

    // 3. 写入输出文件
    std::ofstream output_file(output_filename, std::ios::binary);
    if (!output_file) {
        std::cerr << "错误：无法创建输出文件 '" << output_filename << "'" << std::endl;
        return 1;
    }
    output_file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
    output_file.close();

    std::cout << "已将压缩数据写入到 '" << output_filename << "'" << std::endl;

    return 0;
}
