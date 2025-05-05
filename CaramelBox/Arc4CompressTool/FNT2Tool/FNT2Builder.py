import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import argparse
import zlib
import struct
import codecs
import unicodedata

# FNT2文件头结构
class FNTHDR:
    def __init__(self, height):
        self.sig = b'FNT2'
        self.unk = 0
        self.height = height
        self.comprFlag = 1
    
    def pack(self):
        return struct.pack('<4sIII', self.sig, self.unk, self.height, self.comprFlag)

def zlib_compress(data, level=6):
    """压缩数据使用zlib"""
    return zlib.compress(data, level)

def generate_character_bitmap(char, font, size=24, is_halfwidth=False):
    """
    为单个字符生成位图
    
    参数:
    char -- 要生成的字符
    font -- PIL字体对象
    size -- 字符高度 (默认 24)
    is_halfwidth -- 是否为半角字符
    
    返回:
    bitmap_array -- 字符的灰度位图数组（上下镜像）
    """
    # 对于半角字符，宽度是高度的一半
    width = size // 2 if is_halfwidth else size
    height = size  # 高度保持不变
    
    # 创建一个新的图像，背景为黑色（透明）
    img = Image.new('L', (width, height), color=0)
    draw = ImageDraw.Draw(img)
    
    # 获取字符的边界框
    left, top, right, bottom = draw.textbbox((0, 0), char, font=font)
    text_width = right - left
    text_height = bottom - top
    
    # 获取字体度量信息
    ascent, descent = get_font_metrics(font)
    
    # 计算绘制位置
    x_pos = (width - text_width) // 2 - left
    y_pos = (height - ascent - descent) // 2 - 2
    
    # 绘制字符（白色）
    draw.text((x_pos, y_pos), char, fill=255, font=font)
    
    # 转换为NumPy数组
    bitmap_array = np.array(img)
    
    # 上下镜像
    bitmap_array = np.flipud(bitmap_array)
    
    return bitmap_array

def get_font_metrics(font):
    """获取字体的度量信息，包括上升部分和下降部分"""
    # 创建一个临时图像来获取字体度量
    img = Image.new('L', (100, 100), color=0)
    draw = ImageDraw.Draw(img)
    
    # 使用字符 'Ágjpqy' 来估计字体的上升部分和下降部分
    left, top, right, bottom = draw.textbbox((0, 0), 'Ágjpqy', font=font)
    
    # 使用字符 'x' 来估计x-height
    x_left, x_top, x_right, x_bottom = draw.textbbox((0, 0), 'x', font=font)
    x_height = x_bottom - x_top
    
    # 估计上升部分和下降部分
    ascent = x_height * 1.2  # 估计值，可能需要根据字体调整
    descent = bottom - top - ascent
    
    # 确保值是正的
    ascent = max(1, ascent)
    descent = max(1, descent)
    
    return ascent, descent

def is_halfwidth_char(char):
    """判断字符是否为半角字符"""
    # 获取字符的East Asian Width属性
    ea_width = unicodedata.east_asian_width(char)
    
    # 'Na', 'H' 表示窄字符（半角）
    # 'F', 'W' 表示宽字符（全角）
    # 'A' 表示模糊字符，'N' 表示中性字符
    return ea_width in ['Na', 'H', 'N']

def get_cp932_characters():
    """获取所有CP932（Shift-JIS）字符集中的合法字符"""
    chars = []
    
    # 单字节字符 (ASCII + 半角片假名)
    for i in range(0x20, 0x7F):  # ASCII可打印字符
        chars.append(chr(i))
    for i in range(0xA1, 0xE0):  # 半角片假名和符号
        try:
            char_bytes = bytes([i])
            char = char_bytes.decode('cp932')
            chars.append(char)
        except UnicodeDecodeError:
            pass
    
    # 双字节字符
    # 第一区 (0x8140-0x9FFC)
    for first_byte in range(0x81, 0xA0):
        for second_byte in range(0x40, 0xFD):
            # 跳过不合法的组合
            if second_byte == 0x7F:
                continue
            try:
                char_bytes = bytes([first_byte, second_byte])
                char = char_bytes.decode('cp932')
                chars.append(char)
            except UnicodeDecodeError:
                pass
    
    # 第二区 (0xE040-0xEAA4)
    for first_byte in range(0xE0, 0xFC):
        for second_byte in range(0x40, 0xFD):
            # 跳过不合法的组合
            if second_byte == 0x7F:
                continue
            try:
                char_bytes = bytes([first_byte, second_byte])
                char = char_bytes.decode('cp932')
                chars.append(char)
            except UnicodeDecodeError:
                pass
    
    # 过滤掉控制字符和其他不可见字符
    chars = [c for c in chars if c.isprintable()]
    
    return chars

def char_to_cp932(char):
    """将字符转换为CP932编码"""
    try:
        encoded = char.encode('cp932')
        if len(encoded) == 1:
            return encoded[0]
        elif len(encoded) == 2:
            return (encoded[0] << 8) | encoded[1]
        else:
            return 0
    except UnicodeEncodeError:
        return 0

def generate_fnt2_file(font_path, output_path, size=16):
    """
    生成FNT2格式的字体文件
    
    参数:
    font_path -- 字体文件路径
    output_path -- 输出文件路径
    size -- 字符高度 (默认 16)
    """
    # 加载字体
    font = ImageFont.truetype(font_path, size)
    
    # 获取CP932字符集
    char_set = get_cp932_characters()
    print(f"使用CP932字符集，共 {len(char_set)} 个字符")
    
    # 创建FNT2文件
    with open(output_path, 'wb') as f:
        # 写入文件头
        header = FNTHDR(size)
        f.write(header.pack())
        
        # 为每个字符分配空间
        # 偏移表: 每个字符4字节，共0x10000个
        offset_table_pos = f.tell()
        f.seek(offset_table_pos + 0x10000 * 4)
        
        # 长度表: 每个字符2字节，共0x10000个
        length_table_pos = f.tell()
        f.seek(length_table_pos + 0x10000 * 2)
        
        # 数据区起始位置
        data_start_pos = f.tell()
        
        # 处理每个字符
        processed_count = 0
        failed_count = 0
        current_offset = data_start_pos
        
        for char in char_set:
            # 获取CP932编码
            cp932_code = char_to_cp932(char)
            if cp932_code == 0:
                failed_count += 1
                continue
            
            # 生成字符位图
            try:
                # 判断是否为半角字符
                is_halfwidth = cp932_code < 0x100 or is_halfwidth_char(char)
                
                # 生成位图
                bitmap = generate_character_bitmap(char, font, size, is_halfwidth)
                
                # 压缩位图数据
                compressed_data = zlib_compress(bitmap.tobytes())
                
                # 写入数据
                f.seek(current_offset)
                f.write(compressed_data)
                
                # 更新偏移表
                f.seek(offset_table_pos + cp932_code * 4)
                f.write(struct.pack('<I', current_offset))
                
                # 更新长度表
                f.seek(length_table_pos + cp932_code * 2)
                f.write(struct.pack('<H', len(compressed_data)))
                
                # 更新当前偏移
                current_offset += len(compressed_data)
                
                processed_count += 1
                if processed_count % 100 == 0:
                    print(f"已处理 {processed_count} 个字符...")
            
            except Exception as e:
                print(f"处理字符 '{char}' 时出错: {e}")
                failed_count += 1
    
    print(f"完成! 成功处理了 {processed_count} 个字符。")
    print(f"失败处理了 {failed_count} 个字符。")
    print(f"输出文件: {output_path}")

def main():
    parser = argparse.ArgumentParser(description='从TTF/OTF字体生成FNT2格式字体文件')
    parser.add_argument('font_path', help='字体文件路径')
    parser.add_argument('output_path', help='输出文件路径')
    parser.add_argument('--size', type=int, default=24, help='字符高度 (默认: 24)')
    
    args = parser.parse_args()
    
    generate_fnt2_file(args.font_path, args.output_path, args.size)

if __name__ == "__main__":
    main()
