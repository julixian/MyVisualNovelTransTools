import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import argparse
import codecs
import unicodedata
import math

def is_halfwidth(char):
    """
    判断字符是否为半角字符
    """
    # 使用Unicode东亚宽度属性判断
    # 'Na'/'H' - 窄字符/半角, 'W'/'F' - 宽字符/全角, 'A' - 模糊宽度
    try:
        return unicodedata.east_asian_width(char) in ('Na', 'H')
    except:
        # ASCII 范围内的字符通常是半角
        return ord(char) < 128

def generate_character_bitmap(char, font, output_dir, index, size=16, bits_per_pixel=2):
    """
    为单个字符生成位图文件
    
    参数:
    char -- 要生成的字符
    font -- PIL字体对象
    output_dir -- 输出目录
    index -- 位图文件的索引编号
    size -- 字符高度 (默认 16)
    bits_per_pixel -- 每像素的位数 (默认 2)
    """
    # 判断是否为半角字符，决定宽度
    is_half = is_halfwidth(char)
    width = size // 2 if is_half else size
    
    # 创建一个新的图像，背景为白色
    img = Image.new('L', (width, size), color=255)
    draw = ImageDraw.Draw(img)
    
    # 获取字符的边界框
    left, top, right, bottom = draw.textbbox((0, 0), char, font=font)
    text_width = right - left
    text_height = bottom - top
    
    # 获取字体度量信息
    ascent, descent = get_font_metrics(font)
    
    # 计算基准位置 - 考虑字体的基线
    # 基线位置通常是 ascent 的位置
    baseline = ascent
    x_pos = (width - text_width) // 2 - left
    y_pos = (size - ascent - descent) // 2
    # 绘制字符（黑色）
    draw.text((x_pos, y_pos), char, fill=0, font=font)
    
    # 转换为NumPy数组以便处理
    img_array = np.array(img)
    
    # 将灰度值转换为2位值 (0-3)
    # 255(白色) -> 0(透明), 0(黑色) -> 3(不透明)
    # 中间值按比例映射
    bitmap_array = np.zeros_like(img_array, dtype=np.uint8)
    bitmap_array[img_array > 224] = 0  # 接近白色 -> 0
    bitmap_array[(img_array > 160) & (img_array <= 224)] = 1  # 浅灰色 -> 1
    bitmap_array[(img_array > 96) & (img_array <= 160)] = 2  # 深灰色 -> 2
    bitmap_array[img_array <= 96] = 3  # 接近黑色 -> 3
    
    # 创建输出位图文件
    output_bytes = bytearray()
    
    # 每8个像素组成4个字节 (每像素2位)
    for y in range(size):
        for x in range(0, width, 4):  # 每次处理4个像素 (1字节)
            byte_val = 0
            for i in range(4):
                if x + i < width:
                    # 将2位值打包到一个字节中
                    # 第一个像素在最低位，最后一个在最高位
                    byte_val |= (bitmap_array[y, x + i] & 0x3) << (i * 2)
            output_bytes.append(byte_val)
    
    # 使用索引作为文件名
    file_basename = f"{index:04d}"
    
    # 保存位图文件
    output_file = os.path.join(output_dir, f"{file_basename}.fnt")
    with open(output_file, 'wb') as f:
        f.write(output_bytes)
    
    # 同时保存PNG图像以便查看
    img.save(os.path.join(output_dir, f"{file_basename}.png"))
    
    # 获取Unicode十六进制值用于显示
    unicode_hex = f"{ord(char):04X}"
    
    return output_file, unicode_hex, is_half

def get_font_metrics(font):
    """
    获取字体的度量信息，包括上升部分和下降部分
    """
    # 创建一个临时图像来获取字体度量
    img = Image.new('L', (100, 100), color=255)
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

def generate_cp932_ordered_bitmaps(font_path, output_dir, size=16, batch_size=100, start_index=0):
    """
    按CP932编码顺序生成位图文件
    
    参数:
    font_path -- 字体文件路径
    output_dir -- 输出目录
    size -- 字符高度 (默认 16)
    batch_size -- 每批处理的字符数量 (默认 100)
    start_index -- 开始处理的索引 (默认 0)
    """
    # 确保输出目录存在
    os.makedirs(output_dir, exist_ok=True)
    
    # 加载字体
    font = ImageFont.truetype(font_path, size)
    
    # 定义编码区间
    # 从889F开始，对应索引0618
    encoding_ranges = [
        # 第一区 (0x889F-0x9FFC)
        (0x81, 0x40, 0x00, 0xFF),
        # 第二区 (0xE040-0xFC4B)
        (0xE0, 0xEB, 0x00, 0xFF)
    ]
    
    current_index = 80  # 从889F对应的索引0618开始
    total_processed = 0
    generated_files = []
    
    # 计算总字符数（用于显示进度）
    total_chars = 0
    for first_start, first_end, second_start, second_end in encoding_ranges:
        for first_byte in range(first_start, first_end + 1):
            for second_byte in range(second_start, second_end + 1):
                total_chars += 1
    
    # 特殊处理：从889F开始
    first_byte_start = 0x81
    second_byte_start = 0x40
    
    # 处理第一区
    for first_byte in range(first_byte_start, 0xA0):
        # 特殊处理第一个字节
        if first_byte < 0x89:
            second_range_start = second_byte_start
        else:
            second_range_start = 0x00
        
        for second_byte in range(second_range_start, 256):

            if first_byte == 0x82 and second_byte == 0x4F:
                current_index -= 18
                total_processed -= 18

            if first_byte == 0x83 and second_byte == 0x40:
                current_index -= 14
                total_processed -= 14

            if first_byte == 0x87 and second_byte == 0x40:
                current_index -= 681
                total_processed -= 681

            if first_byte == 0x88 and second_byte == 0x9F:
                current_index -= 194
                total_processed -= 194

            if first_byte == 0x98 and second_byte == 0x9f:
                current_index -= 44
                total_processed -= 44

            try:
                char_bytes = bytes([first_byte, second_byte])
                char = char_bytes.decode('cp932')
                
                # 生成位图
                output_file, unicode_hex, is_half = generate_character_bitmap(char, font, output_dir, current_index, size)
                generated_files.append(output_file)
                width_type = "半角(8x16)" if is_half else "全角(16x16)"
                print(f"已生成 ({total_processed+1}/{total_chars}): {char} (U+{unicode_hex}) [CP932: {first_byte:02X}{second_byte:02X}] [{width_type}] -> {os.path.basename(output_file)}")
                
            except UnicodeDecodeError:
                output_file, unicode_hex, is_half = generate_character_bitmap(char, font, output_dir, current_index, size)
                generated_files.append(output_file)
                print(f"跳过无法编码的字符: {first_byte:02X}{second_byte:02X}")

            # 无论是否成功编码，都增加索引
            current_index += 1
            total_processed += 1
            
            # 每处理batch_size个字符保存一次进度
            if total_processed % batch_size == 0:
                progress_file = os.path.join(output_dir, "progress.txt")
                with open(progress_file, 'w', encoding='utf-8') as f:
                    f.write(f"{total_processed}\n{current_index}")
                print(f"已保存进度: {total_processed}/{total_chars}")

            if first_byte == 0x9f and second_byte == 0xFC:
                break
    
    # 处理第二区 (0xE040-0xFC4B)
    for first_byte in range(0xE0, 0xEB):
        if first_byte == 0xE0:
            second_range_start = 0x40
        else:
            second_range_start = 0x00
        for second_byte in range(second_range_start, 256):
                
            try:
                char_bytes = bytes([first_byte, second_byte])
                char = char_bytes.decode('cp932')
                
                # 生成位图
                output_file, unicode_hex, is_half = generate_character_bitmap(char, font, output_dir, current_index, size)
                generated_files.append(output_file)
                width_type = "半角(8x16)" if is_half else "全角(16x16)"
                print(f"已生成 ({total_processed+1}/{total_chars}): {char} (U+{unicode_hex}) [CP932: {first_byte:02X}{second_byte:02X}] [{width_type}] -> {os.path.basename(output_file)}")
                
            except UnicodeDecodeError:
                output_file, unicode_hex, is_half = generate_character_bitmap(char, font, output_dir, current_index, size)
                generated_files.append(output_file)
                print(f"跳过无法编码的字符: {first_byte:02X}{second_byte:02X}")
            
            # 无论是否成功编码，都增加索引
            current_index += 1
            total_processed += 1
            
            # 每处理batch_size个字符保存一次进度
            if total_processed % batch_size == 0:
                progress_file = os.path.join(output_dir, "progress.txt")
                with open(progress_file, 'w', encoding='utf-8') as f:
                    f.write(f"{total_processed}\n{current_index}")
                print(f"已保存进度: {total_processed}/{total_chars}")
    
    print(f"完成! 已生成 {len(generated_files)} 个字符位图文件到 {output_dir}")
    return generated_files

def main():
    parser = argparse.ArgumentParser(description='按CP932编码顺序从系统字体生成位图字符文件')
    parser.add_argument('font_path', help='字体文件路径')
    parser.add_argument('output_dir', help='输出目录')
    parser.add_argument('--size', type=int, default=16, help='字符高度 (默认: 16)')
    parser.add_argument('--batch-size', type=int, default=100, help='每批处理的字符数量 (默认: 100)')
    parser.add_argument('--resume', action='store_true', help='从上次中断的位置继续')
    
    args = parser.parse_args()
    
    # 确定起始索引
    start_index = 0
    if args.resume:
        progress_file = os.path.join(args.output_dir, "progress.txt")
        if os.path.exists(progress_file):
            with open(progress_file, 'r', encoding='utf-8') as f:
                try:
                    lines = f.readlines()
                    if len(lines) >= 2:
                        start_index = int(lines[1].strip())
                    else:
                        start_index = 0x0618  # 默认从889F对应的索引0618开始
                    print(f"从索引 {start_index} 继续处理")
                except ValueError:
                    print("无法读取进度文件，从头开始")
    
    generate_cp932_ordered_bitmaps(args.font_path, args.output_dir, args.size, args.batch_size, start_index)

if __name__ == "__main__":
    main()
