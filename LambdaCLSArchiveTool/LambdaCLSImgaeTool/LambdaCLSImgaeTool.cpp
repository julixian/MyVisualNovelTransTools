#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <map>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// --- Namespace for CLS -> PNG (Unpack) logic ---
namespace Unpack {
    struct ClsFrameInfo {
        uint32_t width{ 0 }, height{ 0 };
        bool is_compressed{ false };
        int bpp{ 0 }, channels{ 0 };
    };

    struct DecodedImage {
        std::vector<uint8_t> pixels;
        uint32_t width{ 0 }, height{ 0 };
        int channels{ 0 };
    };

    uint16_t swap_endian_16(uint16_t val) { return (val >> 8) | (val << 8); }

    std::vector<uint8_t> decode_rle_channel(std::ifstream& file, uint32_t compressed_size, uint32_t width, uint32_t height) {
        std::vector<uint8_t> channel_data(width * height);
        auto start_of_rle_data = file.tellg();
        std::vector<uint16_t> row_sizes;
        uint32_t total_row_size_bytes = 0;
        while (total_row_size_bytes < compressed_size) {
            uint16_t chunk_size_be;
            file.read(reinterpret_cast<char*>(&chunk_size_be), 2);
            if (file.gcount() < 2) break;
            uint16_t chunk_size = swap_endian_16(chunk_size_be);
            row_sizes.push_back(chunk_size);
            total_row_size_bytes += 2 + chunk_size;
        }
        if (total_row_size_bytes != compressed_size) throw std::runtime_error("RLE row sizes sum mismatch with compressed size.");

        size_t dst_ptr = 0;
        for (uint16_t chunk_size : row_sizes) {
            int width_countdown = width;
            int bytes_read_in_chunk = 0;
            while (bytes_read_in_chunk < chunk_size) {
                uint8_t rle_code; file.read(reinterpret_cast<char*>(&rle_code), 1); bytes_read_in_chunk++;
                if (rle_code < 0x81) {
                    int count = rle_code + 1;
                    if (dst_ptr + count > channel_data.size() || width_countdown < count) throw std::runtime_error("RLE overflow on literal run.");
                    file.read(reinterpret_cast<char*>(&channel_data[dst_ptr]), count);
                    dst_ptr += count; width_countdown -= count; bytes_read_in_chunk += count;
                }
                else {
                    int count = 0x101 - rle_code; uint8_t value; file.read(reinterpret_cast<char*>(&value), 1);
                    if (dst_ptr + count > channel_data.size() || width_countdown < count) throw std::runtime_error("RLE overflow on repeat run.");
                    std::fill_n(&channel_data[dst_ptr], count, value);
                    dst_ptr += count; width_countdown -= count; bytes_read_in_chunk++;
                }
            }
            if (width_countdown > 0) dst_ptr += width_countdown;
        }
        return channel_data;
    }

    DecodedImage decode_frame(std::ifstream& file, uint32_t frame_offset) {
        file.seekg(frame_offset + 0x1C);
        ClsFrameInfo info;
        file.read(reinterpret_cast<char*>(&info.width), 4); file.read(reinterpret_cast<char*>(&info.height), 4);
        file.seekg(frame_offset + 0x30);
        uint8_t compressed_flag; file.read(reinterpret_cast<char*>(&compressed_flag), 1); info.is_compressed = (compressed_flag != 0);
        uint8_t format_code; file.read(reinterpret_cast<char*>(&format_code), 1);
        if (format_code == 2) { info.channels = 1; info.bpp = 8; }
        else if (format_code == 4) { info.channels = 3; info.bpp = 24; }
        else if (format_code == 5) { info.channels = 4; info.bpp = 32; }
        else throw std::runtime_error("Unsupported CLS format code: " + std::to_string(format_code));
        std::vector<uint32_t> channel_offsets(info.channels); std::vector<uint32_t> channel_sizes(info.channels);
        file.seekg(frame_offset + 0x48); file.read(reinterpret_cast<char*>(channel_offsets.data()), info.channels * 4);
        file.seekg(frame_offset + 0x58); file.read(reinterpret_cast<char*>(channel_sizes.data()), info.channels * 4);
        int target_channels = (info.channels == 3) ? 3 : 4;
        std::vector<uint8_t> final_pixels(info.width * info.height * target_channels);
        const int ChannelOrder[] = { 0, 1, 2, 3 };
        if (info.bpp == 8) {
            target_channels = 4; final_pixels.resize(info.width * info.height * target_channels);
            uint32_t palette_offset, palette_size; file.seekg(frame_offset + 0x68); file.read(reinterpret_cast<char*>(&palette_offset), 4); file.read(reinterpret_cast<char*>(&palette_size), 4);
            std::vector<uint8_t> palette(palette_size); file.seekg(frame_offset + palette_offset); file.read(reinterpret_cast<char*>(palette.data()), palette_size);
            std::vector<uint8_t> indices; file.seekg(frame_offset + channel_offsets[0]);
            if (info.is_compressed) {
                uint16_t method_be; file.read(reinterpret_cast<char*>(&method_be), 2); if (swap_endian_16(method_be) != 1) throw std::runtime_error("Unsupported 8bpp compression method");
                indices = decode_rle_channel(file, channel_sizes[0] - 2, info.width, info.height);
            }
            else { indices.resize(info.width * info.height); file.read(reinterpret_cast<char*>(indices.data()), indices.size()); }
            for (size_t i = 0; i < indices.size(); ++i) {
                uint8_t index = indices[i]; final_pixels[i * 4 + 0] = palette[index * 4 + 2]; final_pixels[i * 4 + 1] = palette[index * 4 + 1]; final_pixels[i * 4 + 2] = palette[index * 4 + 0]; final_pixels[i * 4 + 3] = palette[index * 4 + 3];
            }
        }
        else if (!info.is_compressed) {
            std::vector<uint8_t> raw_pixels(info.width * info.height * info.channels); file.seekg(frame_offset + channel_offsets[0]); file.read(reinterpret_cast<char*>(raw_pixels.data()), raw_pixels.size());
            for (size_t i = 0; i < info.width * info.height; ++i) {
                final_pixels[i * info.channels + 0] = raw_pixels[i * info.channels + 2]; final_pixels[i * info.channels + 1] = raw_pixels[i * info.channels + 1]; final_pixels[i * info.channels + 2] = raw_pixels[i * info.channels + 0];
                if (info.channels == 4) final_pixels[i * 4 + 3] = raw_pixels[i * 4 + 3];
            }
        }
        else {
            for (int i = 0; i < info.channels; ++i) {
                file.seekg(frame_offset + channel_offsets[i]); uint16_t method_be; file.read(reinterpret_cast<char*>(&method_be), 2); uint16_t method = swap_endian_16(method_be);
                std::vector<uint8_t> temp_channel_data;
                if (method == 1) { temp_channel_data = decode_rle_channel(file, channel_sizes[i] - 2, info.width, info.height); }
                else if (method == 0) { temp_channel_data.resize(info.width * info.height); file.read(reinterpret_cast<char*>(temp_channel_data.data()), temp_channel_data.size()); }
                else throw std::runtime_error("Unsupported compression method: " + std::to_string(method));
                int png_channel_index = ChannelOrder[i];
                for (size_t p = 0; p < temp_channel_data.size(); ++p) { final_pixels[p * target_channels + png_channel_index] = temp_channel_data[p]; }
            }
            if (info.channels == 3) {
                target_channels = 4; std::vector<uint8_t> rgba_pixels(info.width * info.height * 4);
                for (size_t i = 0; i < info.width * info.height; ++i) {
                    rgba_pixels[i * 4 + 0] = final_pixels[i * 3 + 0]; rgba_pixels[i * 4 + 1] = final_pixels[i * 3 + 1]; rgba_pixels[i * 4 + 2] = final_pixels[i * 3 + 2]; rgba_pixels[i * 4 + 3] = 255;
                }
                final_pixels = rgba_pixels;
            }
        }
        return { final_pixels, info.width, info.height, target_channels };
    }

    void run(int argc, char* argv[]) {
        if (argc != 4) { std::cerr << "Usage for unpack: " << argv[0] << " unpack <input.cls> <output.png | output_folder>" << std::endl; return; }
        const std::string input_path = argv[2]; const std::string output_path_str = argv[3];
        std::ifstream file(input_path, std::ios::binary); if (!file) throw std::runtime_error("Cannot open input file: " + input_path);
        char signature[12] = { 0 }; file.read(signature, 11); if (std::string(signature) != "CLS_TEXFILE") throw std::runtime_error("Not a valid CLS file.");
        uint32_t frame_count; file.seekg(0x10); file.read(reinterpret_cast<char*>(&frame_count), 4);
        if (frame_count == 0) { std::cout << "No frames found in the file." << std::endl; return; }
        std::cout << "File identified. Found " << frame_count << " frames." << std::endl;
        std::vector<uint32_t> frame_offsets; uint32_t first_frame_offset; file.seekg(0x18); file.read(reinterpret_cast<char*>(&first_frame_offset), 4); frame_offsets.push_back(first_frame_offset);
        if (frame_count > 1) {
            uint32_t frame_table_ptr; file.seekg(0x14); file.read(reinterpret_cast<char*>(&frame_table_ptr), 4); file.seekg(frame_table_ptr);
            uint32_t next_frame_offset; file.read(reinterpret_cast<char*>(&next_frame_offset), 4); frame_offsets.push_back(next_frame_offset);
        }
        if (frame_count == 1) {
            DecodedImage image = decode_frame(file, frame_offsets[0]); int stride_in_bytes = image.width * image.channels;
            if (!stbi_write_png(output_path_str.c_str(), image.width, image.height, image.channels, image.pixels.data(), stride_in_bytes)) throw std::runtime_error("Failed to write PNG file.");
            std::cout << "Successfully saved to: " << output_path_str << std::endl;
        }
        else {
            std::filesystem::path output_dir(output_path_str); if (!std::filesystem::exists(output_dir)) { std::filesystem::create_directories(output_dir); }
            for (uint32_t i = 0; i < frame_count; ++i) {
                std::cout << "  - Decoding frame " << (i + 1) << "/" << frame_count << "..." << std::endl;
                DecodedImage image = decode_frame(file, frame_offsets[i]); std::ostringstream filename_stream; filename_stream << std::setw(5) << std::setfill('0') << (i + 1) << ".png";
                std::filesystem::path final_path = output_dir / filename_stream.str(); int stride_in_bytes = image.width * image.channels;
                if (!stbi_write_png(final_path.string().c_str(), image.width, image.height, image.channels, image.pixels.data(), stride_in_bytes)) std::cerr << "Warning: Failed to write file " << final_path << std::endl;
            }
            std::cout << "All frames processed. Saved to directory: " << output_dir << std::endl;
        }
    }
}

// --- Namespace for PNG -> CLS (Pack) logic ---
namespace Pack {
    void write_u32_le_to_stream(std::ostream& os, uint32_t value) { os.write(reinterpret_cast<const char*>(&value), 4); }
    uint32_t read_u32_le(const std::vector<uint8_t>& buffer, size_t offset) { if (offset + 4 > buffer.size()) throw std::out_of_range("Buffer read out of range"); return buffer[offset] | (buffer[offset + 1] << 8) | (buffer[offset + 2] << 16) | (buffer[offset + 3] << 24); }
    struct TemplateFrameInfo { std::vector<uint8_t> header_data; int target_channels; };
    struct FrameData { std::vector<uint8_t> pixels; int width; int height; };

    void run(int argc, char* argv[]) {
        if (argc != 5) { std::cerr << "Usage for pack: " << argv[0] << " pack <input.png | input_folder> <template.cls> <output.cls>" << std::endl; return; }
        const std::filesystem::path input_path(argv[2]); const std::string template_cls_path = argv[3]; const std::string output_cls_path = argv[4];
        std::ifstream template_file(template_cls_path, std::ios::binary); if (!template_file) throw std::runtime_error("Cannot open template CLS file.");
        std::vector<uint8_t> template_buffer((std::istreambuf_iterator<char>(template_file)), std::istreambuf_iterator<char>()); template_file.close();
        uint32_t template_frame_count = read_u32_le(template_buffer, 0x10); if (template_frame_count == 0) throw std::runtime_error("Template file contains no frames.");
        std::vector<TemplateFrameInfo> template_frames; std::vector<uint32_t> template_frame_offsets;
        template_frame_offsets.push_back(read_u32_le(template_buffer, 0x18));
        if (template_frame_count > 1) { uint32_t table_ptr = read_u32_le(template_buffer, 0x14); template_frame_offsets.push_back(read_u32_le(template_buffer, table_ptr)); }
        for (uint32_t offset : template_frame_offsets) {
            TemplateFrameInfo t_info; uint32_t frame_header_size = read_u32_le(template_buffer, offset);
            t_info.header_data.assign(template_buffer.begin() + offset, template_buffer.begin() + offset + frame_header_size);
            uint8_t format_code = template_buffer[offset + 0x31];
            if (format_code == 4) t_info.target_channels = 3; else if (format_code == 5) t_info.target_channels = 4; else throw std::runtime_error("Template contains unsupported frame format (not 24/32bpp).");
            template_frames.push_back(t_info);
        }
        std::vector<std::filesystem::path> png_files;
        if (std::filesystem::is_directory(input_path)) {
            std::map<std::string, std::filesystem::path> sorted_files;
            for (const auto& entry : std::filesystem::directory_iterator(input_path)) { if (entry.path().extension() == ".png") sorted_files[entry.path().filename().string()] = entry.path(); }
            if (sorted_files.empty()) throw std::runtime_error("No .png files found in the input directory.");
            for (const auto& pair : sorted_files) png_files.push_back(pair.second);
        }
        else { png_files.push_back(input_path); }
        uint32_t frames_to_process = png_files.size(); if (frames_to_process == 0) throw std::runtime_error("No input PNG files found.");
        if (frames_to_process > template_frames.size()) { std::cerr << "[Warning] Number of PNG files (" << frames_to_process << ") > template frames (" << template_frames.size() << "). Extra PNGs will be ignored." << std::endl; frames_to_process = template_frames.size(); }
        std::ofstream output_file(output_cls_path, std::ios::binary); if (!output_file) throw std::runtime_error("Cannot create output CLS file.");
        uint32_t header_section_size = (frames_to_process > 1) ? 0x40 : 0x30; output_file.seekp(header_section_size - 1); output_file.write("\0", 1);
        std::vector<uint32_t> final_frame_offsets; std::vector<uint32_t> final_frame_sizes;
        for (uint32_t i = 0; i < frames_to_process; ++i) {
            const auto& png_file = png_files[i]; const auto& t_frame = template_frames[i]; int target_channels = t_frame.target_channels;
            int width, height, channels_in_file; unsigned char* png_pixels_rgba = stbi_load(png_file.string().c_str(), &width, &height, &channels_in_file, 4);
            if (!png_pixels_rgba) throw std::runtime_error("Failed to load PNG file: " + png_file.string());
            FrameData frame_data; frame_data.width = width; frame_data.height = height; frame_data.pixels.resize(width * height * target_channels);
            for (int p = 0; p < width * height; ++p) {
                if (target_channels == 3) { frame_data.pixels[p * 3 + 0] = png_pixels_rgba[p * 4 + 2]; frame_data.pixels[p * 3 + 1] = png_pixels_rgba[p * 4 + 1]; frame_data.pixels[p * 3 + 2] = png_pixels_rgba[p * 4 + 0]; }
                else { frame_data.pixels[p * 4 + 0] = png_pixels_rgba[p * 4 + 2]; frame_data.pixels[p * 4 + 1] = png_pixels_rgba[p * 4 + 1]; frame_data.pixels[p * 4 + 2] = png_pixels_rgba[p * 4 + 0]; frame_data.pixels[p * 4 + 3] = png_pixels_rgba[p * 4 + 3]; }
            }
            stbi_image_free(png_pixels_rgba);
            std::vector<uint8_t> current_frame_header = t_frame.header_data; uint32_t pixel_size = frame_data.pixels.size();
            *reinterpret_cast<uint32_t*>(&current_frame_header[0x0C]) = pixel_size; *reinterpret_cast<uint32_t*>(&current_frame_header[0x10]) = pixel_size;
            *reinterpret_cast<uint32_t*>(&current_frame_header[0x1C]) = width; *reinterpret_cast<uint32_t*>(&current_frame_header[0x20]) = height;
            current_frame_header[0x30] = 0x00; current_frame_header[0x31] = (target_channels == 3) ? 0x04 : 0x05;
            *reinterpret_cast<uint32_t*>(&current_frame_header[0x58]) = pixel_size;
            final_frame_offsets.push_back(output_file.tellp()); final_frame_sizes.push_back(current_frame_header.size() + pixel_size);
            output_file.write(reinterpret_cast<const char*>(current_frame_header.data()), current_frame_header.size());
            output_file.write(reinterpret_cast<const char*>(frame_data.pixels.data()), frame_data.pixels.size());
        }
        uint32_t final_total_size = output_file.tellp(); output_file.seekp(0);
        output_file.write(reinterpret_cast<const char*>(template_buffer.data()), 16);
        write_u32_le_to_stream(output_file, frames_to_process); uint32_t frame_table_offset = 0x20;
        write_u32_le_to_stream(output_file, frame_table_offset); write_u32_le_to_stream(output_file, final_frame_offsets[0]);
        write_u32_le_to_stream(output_file, final_total_size);
        output_file.seekp(frame_table_offset);
        if (frames_to_process == 1) { write_u32_le_to_stream(output_file, final_frame_offsets[0]); write_u32_le_to_stream(output_file, final_frame_sizes[0]); }
        else { write_u32_le_to_stream(output_file, final_frame_offsets[1]); write_u32_le_to_stream(output_file, final_frame_sizes[1]); }
        output_file.close();
        std::cout << "\nConversion successful! " << frames_to_process << " frames written." << std::endl;
    }
}

// --- Main Entry Point ---

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Made by julixian 2025.07.15\n"
            << "Usage:\n"
            << "  cls2png: " << argv[0] << " cls2png <input.cls> <output.png | output_folder>\n"
            << "  png2cls:   " << argv[0] << " png2cls <input.png | input_folder> <template.cls> <output.cls>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    try {
        if (mode == "cls2png") {
            std::cout << "--- Mode: cls2png ---\n" << std::endl;
            Unpack::run(argc, argv);
        }
        else if (mode == "png2cls") {
            std::cout << "--- Mode: png2cls ---\n" << std::endl;
            Pack::run(argc, argv);
        }
        else {
            std::cerr << "Error: Invalid mode '" << mode << "'. Please use 'cls2png' or 'png2cls'.\n\n"
                << "Usage:\n"
                << "  cls2png: " << argv[0] << " cls2png <input.cls> <output.png | output_folder>\n"
                << "  png2cls:   " << argv[0] << " png2cls <input.png | input_folder> <template.cls> <output.cls>" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "\nAn error occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
