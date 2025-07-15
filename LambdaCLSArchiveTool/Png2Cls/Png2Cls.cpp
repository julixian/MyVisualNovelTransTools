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
//struct FrameData {
//    std::vector<uint8_t> pixels;
//    int width;
//    int height;
//};
//
//// --- 主函数 ---
//int main(int argc, char* argv[]) {
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
//        // --- 1. 读取模板CLS并解析所有帧的结构 ---
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
//
//            uint8_t format_code = template_buffer[offset + 0x31];
//            if (format_code == 4) t_info.target_channels = 3;
//            else if (format_code == 5) t_info.target_channels = 4;
//            else throw std::runtime_error("模板中存在不支持的帧格式 (非24/32bpp)。");
//            template_frames.push_back(t_info);
//        }
//
//        // --- 2. 扫描并读取所有输入PNG文件 ---
//        std::vector<std::filesystem::path> png_files;
//        if (std::filesystem::is_directory(input_path)) {
//            std::cout << "检测到输入为文件夹，扫描PNG文件..." << std::endl;
//            std::map<std::string, std::filesystem::path> sorted_files;
//            for (const auto& entry : std::filesystem::directory_iterator(input_path)) {
//                if (entry.path().extension() == ".png") sorted_files[entry.path().filename().string()] = entry.path();
//            }
//            if (sorted_files.empty()) throw std::runtime_error("输入文件夹中未找到任何.png文件。");
//            for (const auto& pair : sorted_files) png_files.push_back(pair.second);
//        }
//        else {
//            std::cout << "检测到输入为文件。" << std::endl;
//            png_files.push_back(input_path);
//        }
//
//        uint32_t frames_to_process = png_files.size();
//        if (frames_to_process == 0) throw std::runtime_error("未找到任何输入PNG文件。");
//        if (frames_to_process > template_frames.size()) {
//            std::cerr << "[警告] PNG文件数量 (" << frames_to_process
//                << ") 大于模板CLS帧数 (" << template_frames.size()
//                << ")。多余的PNG将被忽略。" << std::endl;
//            frames_to_process = template_frames.size();
//        }
//
//        // --- 3. 逐帧处理并直接写入输出文件 ---
//        std::cout << "开始构建新的CLS文件: " << output_cls_path << std::endl;
//        std::ofstream output_file(output_cls_path, std::ios::binary);
//        if (!output_file) throw std::runtime_error("无法创建输出CLS文件。");
//
//        // 预留主文件头(0x20)和帧偏移表(0x10)的空间
//        uint32_t header_section_size = (frames_to_process > 1) ? 0x40 : 0x30;
//        output_file.seekp(header_section_size - 1);
//        output_file.write("\0", 1); // 写入一个字节以确保文件大小
//
//        std::vector<uint32_t> final_frame_offsets;
//        std::vector<uint32_t> final_frame_sizes;
//
//        for (uint32_t i = 0; i < frames_to_process; ++i) {
//            const auto& png_file = png_files[i];
//            const auto& t_frame = template_frames[i];
//            int target_channels = t_frame.target_channels;
//
//            std::cout << "  - 处理帧 " << (i + 1) << ": " << png_file.filename() << " (目标: " << target_channels * 8 << "bpp)" << std::endl;
//
//            int width, height, channels_in_file;
//            unsigned char* png_pixels_rgba = stbi_load(png_file.string().c_str(), &width, &height, &channels_in_file, 4);
//            if (!png_pixels_rgba) throw std::runtime_error("无法加载PNG文件: " + png_file.string());
//
//            FrameData frame_data;
//            frame_data.width = width;
//            frame_data.height = height;
//            frame_data.pixels.resize(width * height * target_channels);
//            for (int p = 0; p < width * height; ++p) {
//                if (target_channels == 3) {
//                    frame_data.pixels[p * 3 + 0] = png_pixels_rgba[p * 4 + 2]; frame_data.pixels[p * 3 + 1] = png_pixels_rgba[p * 4 + 1]; frame_data.pixels[p * 3 + 2] = png_pixels_rgba[p * 4 + 0];
//                }
//                else {
//                    frame_data.pixels[p * 4 + 0] = png_pixels_rgba[p * 4 + 2]; frame_data.pixels[p * 4 + 1] = png_pixels_rgba[p * 4 + 1]; frame_data.pixels[p * 4 + 2] = png_pixels_rgba[p * 4 + 0]; frame_data.pixels[p * 4 + 3] = png_pixels_rgba[p * 4 + 3];
//                }
//            }
//            stbi_image_free(png_pixels_rgba);
//
//            std::vector<uint8_t> current_frame_header = t_frame.header_data;
//            uint32_t pixel_size = frame_data.pixels.size();
//            // ... (修改帧头内容)
//            *reinterpret_cast<uint32_t*>(&current_frame_header[0x0C]) = pixel_size;
//            *reinterpret_cast<uint32_t*>(&current_frame_header[0x10]) = pixel_size;
//            *reinterpret_cast<uint32_t*>(&current_frame_header[0x1C]) = width;
//            *reinterpret_cast<uint32_t*>(&current_frame_header[0x20]) = height;
//            current_frame_header[0x30] = 0x00;
//            current_frame_header[0x31] = (target_channels == 3) ? 0x04 : 0x05;
//            *reinterpret_cast<uint32_t*>(&current_frame_header[0x58]) = pixel_size;
//
//            final_frame_offsets.push_back(output_file.tellp());
//            final_frame_sizes.push_back(current_frame_header.size() + pixel_size);
//
//            output_file.write(reinterpret_cast<const char*>(current_frame_header.data()), current_frame_header.size());
//            output_file.write(reinterpret_cast<const char*>(frame_data.pixels.data()), frame_data.pixels.size());
//        }
//
//        // --- 4. 回写更新主文件头和帧偏移表 ---
//        uint32_t final_total_size = output_file.tellp();
//        output_file.seekp(0);
//
//        std::cout << "回写最终文件头信息..." << std::endl;
//        output_file.write(reinterpret_cast<const char*>(template_buffer.data()), 16);
//        write_u32_le_to_stream(output_file, frames_to_process);
//
//        uint32_t frame_table_offset = 0x20;
//        write_u32_le_to_stream(output_file, frame_table_offset);
//        write_u32_le_to_stream(output_file, final_frame_offsets[0]);
//        write_u32_le_to_stream(output_file, final_total_size);
//
//        // 写入帧偏移表
//        output_file.seekp(frame_table_offset);
//        if (frames_to_process == 1) {
//            write_u32_le_to_stream(output_file, final_frame_offsets[0]);
//            write_u32_le_to_stream(output_file, final_frame_sizes[0]);
//        }
//        else { // frames_to_process >= 2
//            write_u32_le_to_stream(output_file, final_frame_offsets[1]);
//            write_u32_le_to_stream(output_file, final_frame_sizes[1]);
//        }
//
//        output_file.close();
//        std::cout << "\n转换成功！已生成 " << frames_to_process << " 帧的CLS文件。" << std::endl;
//
//    }
//    catch (const std::exception& e) {
//        std::cerr << "\n错误: " << e.what() << std::endl;
//        return 1;
//    }
//
//    return 0;
//}
