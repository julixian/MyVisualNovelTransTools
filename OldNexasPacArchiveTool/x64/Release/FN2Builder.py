#!/usr/bin/env python3
import os
import struct
import argparse
import numpy as np
from PIL import Image

class FN2Encoder:
    """FN2 字体文件编码器"""
    
    def __init__(self, verbose=False):
        self.verbose = verbose
        self.window_size = 1024  # LZ77 滑动窗口大小
        self.min_match_length = 3  # 最小匹配长度
    
    def log(self, message):
        """打印调试信息（如果启用了详细模式）"""
        if self.verbose:
            print(message)
    
    def create_fn2_file(self, font_data, width, height, output_file):
        """创建 FN2 格式字体文件"""
        # 压缩字体数据
        compressed_data = self.compress_lz77(font_data)
        
        if not compressed_data:
            self.log("压缩字体数据失败")
            return False
        
        self.log(f"成功压缩字体数据，原始大小: {len(font_data)} 字节，压缩后: {len(compressed_data)} 字节")
        
        # 写入 FN2 文件
        with open(output_file, 'wb') as f:
            # 写入文件头
            f.write(b'FN2')
            
            # 写入字体尺寸
            f.write(struct.pack('<H', width))
            f.write(struct.pack('<H', height))
            
            # 写入压缩数据
            f.write(compressed_data)
        
        self.log(f"FN2 文件已创建: {output_file}")
        return True
    
    def compress_lz77(self, data):
        """实现 LZ77 压缩算法"""
        if not data:
            return None
        
        # 初始化输出缓冲区
        bitmap = bytearray()  # 位图（0=直接复制，1=回溯引用）
        offset_length_data = bytearray()  # 偏移/长度数据
        
        # 初始化操作计数
        operations_count = 0
        current_bitmap_byte = 0
        current_bit_position = 0
        
        # 处理每个字节
        pos = 0
        while pos < len(data):
            # 查找最佳匹配
            match_offset, match_length = self.find_best_match(data, pos)
            
            # 决定使用直接复制还是回溯引用
            if match_length >= self.min_match_length:
                # 回溯引用
                # 设置位图位
                current_bitmap_byte |= (1 << current_bit_position)
                
                # 编码偏移和长度（高11位=偏移-1，低5位=长度-1）
                offset_length = ((match_offset - 1) << 5) | (match_length - 1)
                offset_length_data.extend(struct.pack('<H', offset_length))
                
                # 更新位置
                pos += match_length
            else:
                # 直接复制
                # 位图位保持为0
                
                # 添加当前字节
                offset_length_data.append(data[pos])
                
                # 更新位置
                pos += 1
            
            # 更新位图位置
            current_bit_position += 1
            if current_bit_position == 8:
                bitmap.append(current_bitmap_byte)
                current_bitmap_byte = 0
                current_bit_position = 0
            
            # 更新操作计数
            operations_count += 1
        
        # 处理最后一个不完整的字节
        if current_bit_position > 0:
            bitmap.append(current_bitmap_byte)
        
        # 构建完整的压缩数据
        result = bytearray()
        
        # 写入解压后大小
        result.extend(struct.pack('<I', len(data)))
        
        # 写入操作数量
        result.extend(struct.pack('<I', operations_count))
        
        # 写入位图
        result.extend(bitmap)
        
        # 写入偏移/长度数据大小
        result.extend(struct.pack('<I', len(offset_length_data)))
        
        # 写入偏移/长度数据
        result.extend(offset_length_data)
        
        return bytes(result)
    
    def find_best_match(self, data, current_pos):
        """查找最佳匹配（最长的重复序列）"""
        # 确定窗口范围
        window_start = max(0, current_pos - self.window_size)
        
        # 初始化最佳匹配
        best_match_length = 0
        best_match_offset = 0
        
        # 在窗口中查找匹配
        for i in range(window_start, current_pos):
            # 计算当前位置的最大可能匹配长度
            max_length = min(len(data) - current_pos, 32)  # 最多匹配32字节（5位长度字段）
            
            # 计算实际匹配长度
            match_length = 0
            while (match_length < max_length and 
                   data[i + match_length] == data[current_pos + match_length]):
                match_length += 1
            
            # 更新最佳匹配
            if match_length > best_match_length:
                best_match_length = match_length
                best_match_offset = current_pos - i
                
                # 如果找到最大可能长度的匹配，立即返回
                if best_match_length == max_length:
                    break
        
        return best_match_offset, best_match_length
    
    def load_font_data(self, input_file):
        """加载字体数据"""
        try:
            with open(input_file, 'rb') as f:
                data = f.read()
            self.log(f"已加载字体数据，大小: {len(data)} 字节")
            return data
        except Exception as e:
            self.log(f"加载字体数据失败: {e}")
            return None
    
    def load_font_image(self, input_file, width, height):
        """从图像加载字体数据"""
        try:
            img = Image.open(input_file).convert('L')
            
            # 调整图像大小（如果需要）
            if img.width != width or img.height != height:
                self.log(f"调整图像大小从 {img.width}x{img.height} 到 {width}x{height}")
                img = img.resize((width, height), Image.LANCZOS)
            
            # 转换为NumPy数组
            img_array = np.array(img)
            
            # 转换为2位/像素格式
            return self.encode_2bit_bitmap(img_array, width, height)
        except Exception as e:
            self.log(f"加载字体图像失败: {e}")
            return None
    
    def encode_2bit_bitmap(self, img_array, width, height):
        """将图像编码为2位/像素位图"""
        # 创建输出缓冲区
        output = bytearray()
        
        # 将灰度值转换为2位值
        # 0-63 -> 0, 64-127 -> 1, 128-191 -> 2, 192-255 -> 3
        quantized = (img_array // 64).astype(np.uint8)
        
        # 遍历每一行
        for y in range(height):
            # 遍历每组4个像素
            for x in range(0, width, 4):
                byte_val = 0
                
                # 打包4个像素到一个字节
                for i in range(4):
                    if x + i < width:
                        # 获取2位值 (0-3)
                        pixel_val = quantized[y, x + i]
                        
                        # 将2位值放入字节中
                        byte_val |= (pixel_val & 0x3) << (i * 2)
                
                output.append(byte_val)
        
        return bytes(output)
    
    def load_font_sheet(self, input_file, char_width, char_height, num_chars=94):
        """从字体表图像加载字体数据"""
        try:
            img = Image.open(input_file).convert('L')
            
            # 计算字体表的行列数
            cols = 16  # 假设每行16个字符
            rows = (num_chars + cols - 1) // cols
            
            # 检查图像尺寸
            expected_width = cols * char_width
            expected_height = rows * char_height
            
            if img.width != expected_width or img.height != expected_height:
                self.log(f"警告: 图像尺寸 ({img.width}x{img.height}) 与预期 ({expected_width}x{expected_height}) 不符")
                
                # 如果图像太大，裁剪它
                if img.width > expected_width and img.height > expected_height:
                    img = img.crop((0, 0, expected_width, expected_height))
                    self.log(f"已裁剪图像至 {expected_width}x{expected_height}")
                else:
                    # 调整图像大小
                    img = img.resize((expected_width, expected_height), Image.LANCZOS)
                    self.log(f"已调整图像大小至 {expected_width}x{expected_height}")
            
            # 转换为NumPy数组
            img_array = np.array(img)
            
            # 提取每个字符并编码
            all_chars_data = bytearray()
            
            for i in range(min(num_chars, rows * cols)):
                row = i // cols
                col = i % cols
                
                y_start = row * char_height
                x_start = col * char_width
                
                # 提取字符图像
                char_img = img_array[y_start:y_start+char_height, x_start:x_start+char_width]
                
                # 编码为2位/像素位图
                char_data = self.encode_2bit_bitmap(char_img, char_width, char_height)
                
                # 添加到总数据
                all_chars_data.extend(char_data)
            
            self.log(f"已从字体表提取 {min(num_chars, rows * cols)} 个字符")
            return bytes(all_chars_data)
        except Exception as e:
            self.log(f"加载字体表失败: {e}")
            return None

def main():
    parser = argparse.ArgumentParser(description='创建 FN2 格式字体文件')
    parser.add_argument('--bin', help='输入的字体数据文件 (.bin)')
    parser.add_argument('--image', help='输入的字体图像文件 (.png)')
    parser.add_argument('--sheet', help='输入的字体表图像文件 (.png)')
    parser.add_argument('--output', required=True, help='输出的 FN2 文件路径')
    parser.add_argument('--width', type=int, default=16, help='字体宽度 (默认: 16)')
    parser.add_argument('--height', type=int, default=16, help='字体高度 (默认: 16)')
    parser.add_argument('--chars', type=int, default=94, help='字符数量 (默认: 94)')
    parser.add_argument('-v', '--verbose', action='store_true', help='启用详细输出')
    
    args = parser.parse_args()
    
    if not (args.bin or args.image or args.sheet):
        print("错误: 必须提供输入文件 (--bin, --image 或 --sheet)")
        return
    
    encoder = FN2Encoder(verbose=args.verbose)
    
    try:
        # 加载字体数据
        font_data = None
        
        if args.bin:
            print(f"从二进制文件加载字体数据: {args.bin}")
            font_data = encoder.load_font_data(args.bin)
        elif args.image:
            print(f"从图像文件加载字体数据: {args.image}")
            font_data = encoder.load_font_image(args.image, args.width, args.height)
        elif args.sheet:
            print(f"从字体表图像加载字体数据: {args.sheet}")
            font_data = encoder.load_font_sheet(args.sheet, args.width, args.height, args.chars)
        
        if not font_data:
            print("加载字体数据失败")
            return
        
        # 创建 FN2 文件
        if encoder.create_fn2_file(font_data, args.width, args.height, args.output):
            print(f"FN2 文件已成功创建: {args.output}")
            print(f"字体尺寸: {args.width}x{args.height} 像素")
        else:
            print("创建 FN2 文件失败")
    except Exception as e:
        print(f"处理过程中出错: {e}")

if __name__ == "__main__":
    main()
