#!/usr/bin/env python3
import os
import struct
import argparse
import numpy as np
from PIL import Image

class FN2Decoder:
    """FN2 字体文件解码器"""
    
    def __init__(self, verbose=False):
        self.verbose = verbose
    
    def log(self, message):
        """打印调试信息（如果启用了详细模式）"""
        if self.verbose:
            print(message)
    
    def read_fn2_file(self, filepath):
        """读取并解析 FN2 字体文件"""
        with open(filepath, 'rb') as f:
            # 检查文件头
            header = f.read(3)
            if header != b'FN2':
                raise ValueError(f"无效的 FN2 文件: {filepath}，文件头应为 'FN2'，实际为 {header}")
            
            self.log(f"有效的 FN2 文件头: {header.decode('ascii')}")
            
            # 读取字体尺寸
            width_bytes = f.read(2)
            height_bytes = f.read(2)
            width = struct.unpack('<H', width_bytes)[0]
            height = struct.unpack('<H', height_bytes)[0]
            
            self.log(f"字体尺寸: {width}x{height} 像素")
            
            # 解压字体数据
            font_data = self.decompress_lz77(f)
            
            if font_data:
                self.log(f"成功解压字体数据，大小: {len(font_data)} 字节")
                
                # 计算预期数据大小（2位/像素）
                expected_size = (width * height * 2 + 7) // 8
                self.log(f"预期数据大小: {expected_size} 字节")
                
                return {
                    'width': width,
                    'height': height,
                    'data': font_data
                }
            else:
                self.log("解压字体数据失败")
                return None
    
    def decompress_lz77(self, f):
        """实现 LZ77 解压缩算法（与 COD 文件解压相同）"""
        try:
            # 读取解压后大小
            uncompressed_size_bytes = f.read(4)
            if not uncompressed_size_bytes or len(uncompressed_size_bytes) < 4:
                return None
            
            uncompressed_size = struct.unpack('<I', uncompressed_size_bytes)[0]
            self.log(f"解压后大小: {uncompressed_size} 字节")
            
            # 读取操作数量
            operations_count_bytes = f.read(4)
            if not operations_count_bytes or len(operations_count_bytes) < 4:
                return None
            
            operations_count = struct.unpack('<I', operations_count_bytes)[0]
            self.log(f"操作数量: {operations_count}")
            
            # 读取位图
            bitmap_size = (operations_count + 7) // 8
            bitmap_bytes = f.read(bitmap_size)
            if not bitmap_bytes or len(bitmap_bytes) < bitmap_size:
                return None
            
            self.log(f"位图大小: {bitmap_size} 字节")
            
            # 读取偏移/长度数据大小
            offset_length_size_bytes = f.read(4)
            if not offset_length_size_bytes or len(offset_length_size_bytes) < 4:
                return None
            
            offset_length_size = struct.unpack('<I', offset_length_size_bytes)[0]
            self.log(f"偏移/长度数据大小: {offset_length_size} 字节")
            
            # 读取偏移/长度数据
            offset_length_data = f.read(offset_length_size)
            if not offset_length_data or len(offset_length_data) < offset_length_size:
                return None
            
            # 执行解压缩
            return self.lz77_decompress(
                bitmap_bytes, 
                offset_length_data, 
                operations_count, 
                uncompressed_size
            )
            
        except Exception as e:
            self.log(f"解压过程中出错: {e}")
            return None
    
    def lz77_decompress(self, bitmap, offset_length_data, operations_count, uncompressed_size):
        """实现 LZ77 解压缩算法"""
        output = bytearray(uncompressed_size)
        output_pos = 0
        offset_length_pos = 0
        
        for op_idx in range(operations_count):
            if output_pos >= uncompressed_size:
                break
                
            # 检查位图中的对应位
            byte_idx = op_idx // 8
            bit_idx = op_idx % 8
            is_backreference = (bitmap[byte_idx] & (1 << bit_idx)) != 0
            
            if is_backreference:
                # 回溯引用
                if offset_length_pos + 1 >= len(offset_length_data):
                    break
                    
                # 读取偏移和长度
                offset_length = struct.unpack('<H', offset_length_data[offset_length_pos:offset_length_pos+2])[0]
                offset = (offset_length >> 5) + 1
                length = (offset_length & 0x1F) + 1
                offset_length_pos += 2
                
                # 复制之前的数据
                for i in range(length):
                    if output_pos >= uncompressed_size or output_pos - offset < 0:
                        break
                    output[output_pos] = output[output_pos - offset]
                    output_pos += 1
            else:
                # 直接复制
                if offset_length_pos >= len(offset_length_data):
                    break
                    
                output[output_pos] = offset_length_data[offset_length_pos]
                offset_length_pos += 1
                output_pos += 1
        
        return bytes(output)
    
    def extract_character_bitmaps(self, font_data, width, height, num_chars=94):
        """从字体数据中提取字符位图"""
        # 计算每个字符所需的字节数（2位/像素）
        bytes_per_char = (width * height * 2 + 7) // 8
        
        # 检查数据大小是否足够
        if len(font_data) < bytes_per_char * num_chars:
            self.log(f"警告: 字体数据大小 ({len(font_data)} 字节) 小于预期 ({bytes_per_char * num_chars} 字节)")
            num_chars = len(font_data) // bytes_per_char
            
        self.log(f"提取 {num_chars} 个字符，每个字符 {bytes_per_char} 字节")
        
        # 提取每个字符的位图
        char_bitmaps = []
        for i in range(num_chars):
            start_pos = i * bytes_per_char
            end_pos = start_pos + bytes_per_char
            
            if end_pos > len(font_data):
                break
                
            char_data = font_data[start_pos:end_pos]
            bitmap = self.decode_2bit_bitmap(char_data, width, height)
            char_bitmaps.append(bitmap)
            
        return char_bitmaps
    
    def decode_2bit_bitmap(self, data, width, height):
        """解码2位/像素的字符位图"""
        # 创建空白位图
        bitmap = np.zeros((height, width), dtype=np.uint8)
        
        # 遍历每个字节
        byte_idx = 0
        for y in range(height):
            for x in range(0, width, 4):  # 每字节4个像素
                if byte_idx >= len(data):
                    break
                    
                byte_val = data[byte_idx]
                byte_idx += 1
                
                # 从字节中提取4个像素
                for i in range(4):
                    if x + i >= width:
                        break
                        
                    # 提取2位值 (0-3)
                    pixel_val = (byte_val >> (i * 2)) & 0x3
                    
                    # 将2位值 (0-3) 转换为8位灰度值 (0-255)
                    gray_val = pixel_val * 85  # 0, 85, 170, 255
                    
                    bitmap[y, x + i] = gray_val
        
        return bitmap
    
    def create_font_sheet(self, char_bitmaps, width, height, cols=16):
        """创建字体表图像"""
        if not char_bitmaps:
            return None
            
        # 计算行数
        num_chars = len(char_bitmaps)
        rows = (num_chars + cols - 1) // cols
        
        # 创建空白图像
        sheet = np.zeros((rows * height, cols * width), dtype=np.uint8)
        
        # 填充字符位图
        for i, bitmap in enumerate(char_bitmaps):
            if i >= rows * cols:
                break
                
            row = i // cols
            col = i % cols
            
            y_start = row * height
            x_start = col * width
            
            sheet[y_start:y_start+height, x_start:x_start+width] = bitmap
        
        return sheet
    
    def save_font_data(self, font_info, output_dir):
        """保存字体数据和图像"""
        os.makedirs(output_dir, exist_ok=True)
        
        # 保存原始字体数据
        with open(os.path.join(output_dir, 'font_data.bin'), 'wb') as f:
            f.write(font_info['data'])
        
        # 提取字符位图
        char_bitmaps = self.extract_character_bitmaps(
            font_info['data'], 
            font_info['width'], 
            font_info['height']
        )
        
        # 保存单个字符图像
        for i, bitmap in enumerate(char_bitmaps):
            img = Image.fromarray(bitmap)
            img.save(os.path.join(output_dir, f'char_{i:03d}.png'))
        
        # 创建并保存字体表图像
        sheet = self.create_font_sheet(char_bitmaps, font_info['width'], font_info['height'])
        if sheet is not None:
            sheet_img = Image.fromarray(sheet)
            sheet_img.save(os.path.join(output_dir, 'font_sheet.png'))
        
        self.log(f"已保存 {len(char_bitmaps)} 个字符图像和字体表")

def main():
    parser = argparse.ArgumentParser(description='解析 FN2 格式字体文件')
    parser.add_argument('input_file', help='输入的 FN2 文件路径')
    parser.add_argument('output_dir', help='输出目录')
    parser.add_argument('-v', '--verbose', action='store_true', help='启用详细输出')
    
    args = parser.parse_args()
    
    decoder = FN2Decoder(verbose=args.verbose)
    
    try:
        print(f"开始处理 FN2 文件: {args.input_file}")
        font_info = decoder.read_fn2_file(args.input_file)
        
        if font_info:
            decoder.save_font_data(font_info, args.output_dir)
            print(f"FN2 文件解析完成，结果保存在: {args.output_dir}")
            print(f"字体尺寸: {font_info['width']}x{font_info['height']} 像素")
        else:
            print("解析 FN2 文件失败")
    except Exception as e:
        print(f"处理 FN2 文件时出错: {e}")

if __name__ == "__main__":
    main()
