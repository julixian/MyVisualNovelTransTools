//// ... (所有 include 和辅助函数保持不变) ...
//#define _CRT_SECURE_NO_WARNINGS
//#include <iostream>
//#include <fstream>
//#include <vector>
//#include <cstdint>
//#include <string>
//#include <stdexcept>
//#include <algorithm>
//#include <filesystem>
//#include <iomanip>
//#include <sstream>
//#include <map>
//
//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"
//
//// 辅助函数
//void write_u32_le_to_stream(std::ostream& os, uint32_t value) {
//    os.write(reinterpret_cast<const char*>(&value), 4);
//}
//
//void write_u16_be_to_stream(std::ostream& os, uint16_t value) {
//    uint16_t be_val = (value >> 8) | (value << 8);
//    os.write(reinterpret_cast<const char*>(&be_val), 2);
//}
//
//uint32_t read_u32_le(const std::vector<uint8_t>& buffer, size_t offset) {
//    if (offset + 4 > buffer.size()) throw std::out_of_range("从缓冲区读取时发生越界");
//    return buffer[offset] | (buffer[offset + 1] << 8) | (buffer[offset + 2] << 16) | (buffer[offset + 3] << 24);
//}
//
//// 结构体
//struct TemplateFrameInfo {
//    std::vector<uint8_t> header_data;
//    int target_channels;
//};
//
//struct ProcessedFrameData {
//    std::vector<std::vector<uint8_t>> channels_pixels; // B, G, R, A
//    int width;
//    int height;
//};
//
//
//// --- 新增：真正的RLE编码函数 ---
//std::vector<uint8_t> encode_rle_channel(const std::vector<uint8_t>& channel_data, uint32_t width, uint32_t height) {
//    std::vector<uint8_t> compressed_data;
//    std::vector<uint16_t> row_sizes;
//
//    // 1. 逐行进行RLE编码
//    for (uint32_t y = 0; y < height; ++y) {
//        std::vector<uint8_t> compressed_row;
//        const uint8_t* row_start = &channel_data[y * width];
//        uint32_t pos = 0;
//
//        while (pos < width) {
//            // 寻找重复运行
//            uint32_t run_length = 1;
//            while (pos + run_length < width && run_length < 128 && row_start[pos] == row_start[pos + run_length]) {
//                run_length++;
//            }
//
//            if (run_length > 1) { // 发现重复运行，进行编码
//                compressed_row.push_back(257 - run_length);
//                compressed_row.push_back(row_start[pos]);
//                pos += run_length;
//            }
//            else {
//                // 未发现重复，寻找原文运行
//                uint32_t literal_start = pos;
//                pos++;
//                while (pos < width) {
//                    // 向前看，如果接下来有两个连续相同的字节，则停止原文运行
//                    if (pos + 1 < width && row_start[pos] == row_start[pos + 1]) {
//                        break;
//                    }
//                    // 如果原文运行长度达到128，也停止
//                    if (pos - literal_start >= 128) {
//                        break;
//                    }
//                    pos++;
//                }
//                uint32_t literal_length = pos - literal_start;
//                compressed_row.push_back(literal_length - 1);
//                compressed_row.insert(compressed_row.end(), &row_start[literal_start], &row_start[literal_start + literal_length]);
//            }
//        }
//        row_sizes.push_back(compressed_row.size());
//        compressed_data.insert(compressed_data.end(), compressed_row.begin(), compressed_row.end());
//    }
//
//    // 2. 将行大小列表预置到最终数据的开头
//    std::vector<uint8_t> final_data;
//    final_data.reserve(row_sizes.size() * 2 + compressed_data.size());
//    for (uint16_t size : row_sizes) {
//        uint16_t be_size = (size >> 8) | (size << 8);
//        final_data.push_back(be_size & 0xFF);
//        final_data.push_back(be_size >> 8);
//    }
//    final_data.insert(final_data.end(), compressed_data.begin(), compressed_data.end());
//
//    return final_data;
//}
//
//
//// --- 主函数 (大部分不变，只修改调用部分) ---
//int main(int argc, char* argv[]) {
//    // ... (主函数前半部分与上一版本完全相同) ...
//    if (argc != 4) {
//        std::cerr << "用法:\n"
//            << "  单帧: " << argv[0] << " <输入.png文件> <模板.cls文件> <输出.cls文件>\n"
//            << "  多帧: " << argv[0] << " <输入文件夹> <模板.cls文件> <输出.cls文件>" << std::endl;
//        return 1;
//    }
//
//    const std::filesystem::path input_path(argv[1]);
//    const std::string template_cls_path = argv[2];
//    const std::string output_cls_path = argv[3];
//
//    try {
//        // --- 1. 读取模板CLS并解析 ---
//        std::cout << "正在读取模板CLS文件: " << template_cls_path << std::endl;
//        std::ifstream template_file(template_cls_path, std::ios::binary);
//        if (!template_file) throw std::runtime_error("无法打开模板CLS文件。");
//        std::vector<uint8_t> template_buffer((std::istreambuf_iterator<char>(template_file)), std::istreambuf_iterator<char>());
//        template_file.close();
//
//        uint32_t template_frame_count = read_u32_le(template_buffer, 0x10);
//        if (template_frame_count == 0) throw std::runtime_error("模板文件不包含任何帧。");
//
//        std::vector<TemplateFrameInfo> template_frames;
//        std::vector<uint32_t> template_frame_offsets;
//        template_frame_offsets.push_back(read_u32_le(template_buffer, 0x18));
//        if (template_frame_count > 1) {
//            uint32_t table_ptr = read_u32_le(template_buffer, 0x14);
//            template_frame_offsets.push_back(read_u32_le(template_buffer, table_ptr));
//        }
//
//        std::cout << "  解析模板中的 " << template_frame_count << " 帧结构..." << std::endl;
//        for (uint32_t offset : template_frame_offsets) {
//            TemplateFrameInfo t_info;
//            uint32_t frame_header_size = read_u32_le(template_buffer, offset);
//            t_info.header_data.assign(template_buffer.begin() + offset, template_buffer.begin() + offset + frame_header_size);
//            uint8_t format_code = template_buffer[offset + 0x31];
//            if (format_code == 4) t_info.target_channels = 3; else if (format_code == 5) t_info.target_channels = 4;
//            else throw std::runtime_error("模板中存在不支持的帧格式 (非24/32bpp)。");
//            template_frames.push_back(t_info);
//        }
//
//        // --- 2. 扫描并读取所有输入PNG文件 ---
//        std::vector<std::filesystem::path> png_files;
//        if (std::filesystem::is_directory(input_path)) {
//            std::map<std::string, std::filesystem::path> sorted_files;
//            for (const auto& entry : std::filesystem::directory_iterator(input_path)) if (entry.path().extension() == ".png") sorted_files[entry.path().filename().string()] = entry.path();
//            if (sorted_files.empty()) throw std::runtime_error("输入文件夹中未找到任何.png文件。");
//            for (const auto& pair : sorted_files) png_files.push_back(pair.second);
//        }
//        else {
//            png_files.push_back(input_path);
//        }
//
//        uint32_t frames_to_process = png_files.size();
//        if (frames_to_process > template_frames.size()) {
//            std::cerr << "[警告] PNG文件数量 (" << frames_to_process << ") > 模板帧数 (" << template_frames.size() << ")。多余PNG将被忽略。" << std::endl;
//            frames_to_process = template_frames.size();
//        }
//
//        // --- 3. 逐帧处理并直接写入输出文件 ---
//        std::cout << "开始构建新的RLE压缩CLS文件: " << output_cls_path << std::endl;
//        std::ofstream output_file(output_cls_path, std::ios::binary);
//        if (!output_file) throw std::runtime_error("无法创建输出CLS文件。");
//
//        uint32_t header_section_size = (frames_to_process > 1) ? 0x40 : 0x30;
//        output_file.seekp(header_section_size - 1);
//        output_file.write("\0", 1);
//
//        std::vector<uint32_t> final_frame_offsets;
//        std::vector<uint32_t> final_frame_sizes;
//        std::vector<std::vector<uint8_t>> all_frame_headers;
//        std::vector<std::vector<std::vector<uint8_t>>> all_compressed_channels_data; // 存储所有帧的所有压缩后通道数据
//
//        for (uint32_t i = 0; i < frames_to_process; ++i) {
//            const auto& png_file = png_files[i];
//            const auto& t_frame = template_frames[i];
//            int target_channels = t_frame.target_channels;
//
//            std::cout << "  - 处理并压缩帧 " << (i + 1) << ": " << png_file.filename() << std::endl;
//
//            int width, height, channels_in_file;
//            unsigned char* png_pixels_rgba = stbi_load(png_file.string().c_str(), &width, &height, &channels_in_file, 4);
//            if (!png_pixels_rgba) throw std::runtime_error("无法加载PNG文件: " + png_file.string());
//
//            // 分离通道
//            std::vector<std::vector<uint8_t>> separated_channels(target_channels);
//            for (auto& chan_vec : separated_channels) chan_vec.resize(width * height);
//            for (int p = 0; p < width * height; ++p) {
//                separated_channels[0][p] = png_pixels_rgba[p * 4 + 2]; // B
//                separated_channels[1][p] = png_pixels_rgba[p * 4 + 1]; // G
//                separated_channels[2][p] = png_pixels_rgba[p * 4 + 0]; // R
//                if (target_channels == 4) separated_channels[3][p] = png_pixels_rgba[p * 4 + 3]; // A
//            }
//            stbi_image_free(png_pixels_rgba);
//
//            // RLE压缩每个通道
//            std::vector<std::vector<uint8_t>> compressed_channels;
//            for (int c = 0; c < target_channels; ++c) {
//                compressed_channels.push_back(encode_rle_channel(separated_channels[c], width, height));
//            }
//            all_compressed_channels_data.push_back(compressed_channels);
//
//            // 准备帧头
//            std::vector<uint8_t> current_frame_header = t_frame.header_data;
//            *reinterpret_cast<uint32_t*>(&current_frame_header[0x1C]) = width;
//            *reinterpret_cast<uint32_t*>(&current_frame_header[0x20]) = height;
//            current_frame_header[0x30] = 0x01; // IsCompressed = true
//            current_frame_header[0x31] = (target_channels == 3) ? 0x04 : 0x05;
//            all_frame_headers.push_back(current_frame_header);
//
//            // 写入帧头和压缩后的数据
//            final_frame_offsets.push_back(output_file.tellp());
//            output_file.write(reinterpret_cast<const char*>(current_frame_header.data()), current_frame_header.size());
//
//            uint32_t total_pixel_data_size_for_frame = 0;
//            for (const auto& chan_data : compressed_channels) {
//                write_u16_be_to_stream(output_file, 0x0001); // RLE压缩方法代码: 1
//                output_file.write(reinterpret_cast<const char*>(chan_data.data()), chan_data.size());
//                total_pixel_data_size_for_frame += 2 + chan_data.size();
//            }
//            final_frame_sizes.push_back(current_frame_header.size() + total_pixel_data_size_for_frame);
//        }
//
//        // --- 4. 回写更新所有头信息 ---
//        uint32_t final_total_size = output_file.tellp();
//
//        std::cout << "回写最终文件头信息..." << std::endl;
//        output_file.seekp(0);
//        output_file.write(reinterpret_cast<const char*>(template_buffer.data()), 16);
//        write_u32_le_to_stream(output_file, frames_to_process);
//        uint32_t frame_table_offset = 0x20;
//        write_u32_le_to_stream(output_file, frame_table_offset);
//        write_u32_le_to_stream(output_file, final_frame_offsets[0]);
//        write_u32_le_to_stream(output_file, final_total_size);
//
//        if (frames_to_process > 1) {
//            output_file.seekp(frame_table_offset);
//            write_u32_le_to_stream(output_file, final_frame_offsets[1]);
//            write_u32_le_to_stream(output_file, final_frame_sizes[1]);
//        }
//        else {
//            output_file.seekp(frame_table_offset);
//            write_u32_le_to_stream(output_file, final_frame_offsets[0]);
//            write_u32_le_to_stream(output_file, final_frame_sizes[0]);
//        }
//
//        for (uint32_t i = 0; i < frames_to_process; ++i) {
//            uint32_t frame_offset = final_frame_offsets[i];
//            uint32_t frame_header_size = all_frame_headers[i].size();
//            uint32_t running_channel_offset = frame_header_size;
//
//            output_file.seekp(frame_offset + 0x48); // offsets table
//            for (const auto& chan_data : all_compressed_channels_data[i]) {
//                write_u32_le_to_stream(output_file, running_channel_offset);
//                running_channel_offset += 2 + chan_data.size();
//            }
//
//            output_file.seekp(frame_offset + 0x58); // sizes table
//            for (const auto& chan_data : all_compressed_channels_data[i]) {
//                write_u32_le_to_stream(output_file, 2 + chan_data.size());
//            }
//        }
//
//        output_file.close();
//        std::cout << "\n转换成功！已生成 " << frames_to_process << " 帧的RLE压缩CLS文件。" << std::endl;
//
//    }
//    catch (const std::exception& e) {
//        std::cerr << "\n错误: " << e.what() << std::endl;
//        return 1;
//    }
//
//    return 0;
//}
