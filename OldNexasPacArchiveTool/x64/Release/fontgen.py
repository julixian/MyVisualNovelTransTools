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

def generate_character_bitmap(char, font, output_dir, size=16, bits_per_pixel=2):
    """
    为单个字符生成位图文件
    
    参数:
    char -- 要生成的字符
    font -- PIL字体对象
    output_dir -- 输出目录
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
    
    # 使用Unicode十六进制值作为文件名
    unicode_hex = f"{ord(char):04X}"  # 至少4位，不足补0
    
    # 根据字符宽度设置文件名
    if is_half:
        file_basename = f"{unicode_hex}_5x10"
    else:
        file_basename = f"{unicode_hex}_10x10"
    
    # 保存位图文件
    output_file = os.path.join(output_dir, f"{file_basename}.fnt")
    with open(output_file, 'wb') as f:
        f.write(output_bytes)
    
    # 同时保存PNG图像以便查看
    img.save(os.path.join(output_dir, f"{file_basename}.png"))
    
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

def generate_character_set(font_path, output_dir, char_set=None, size=16, batch_size=100, start_index=0):
    """
    从字体生成一组字符的位图文件
    
    参数:
    font_path -- 字体文件路径
    output_dir -- 输出目录
    char_set -- 要生成的字符集 (默认为None，将使用CP932字符集)
    size -- 字符高度 (默认 16)
    batch_size -- 每批处理的字符数量 (默认 100)
    start_index -- 开始处理的字符索引 (默认 0)
    """
    # 确保输出目录存在
    os.makedirs(output_dir, exist_ok=True)
    
    # 加载字体
    font = ImageFont.truetype(font_path, size)
    
    # 如果未提供字符集，使用CP932字符集
    if char_set is None:
        char_set = get_cp932_characters()
        print(f"使用CP932字符集，共 {len(char_set)} 个字符")
    
    # 处理每个字符
    generated_files = []
    total_chars = len(char_set)
    
    # 从指定索引开始处理
    for i in range(start_index, total_chars):
        char = char_set[i]
        try:
            output_file, unicode_hex, is_half = generate_character_bitmap(char, font, output_dir, size)
            generated_files.append(output_file)
            width_type = "半角(8x16)" if is_half else "全角(16x16)"
            print(f"已生成 ({i+1}/{total_chars}): {char} (U+{unicode_hex}) [{width_type}] -> {os.path.basename(output_file)}")
            
            # 每处理batch_size个字符保存一次进度
            if (i + 1) % batch_size == 0:
                progress_file = os.path.join(output_dir, "progress.txt")
                with open(progress_file, 'w', encoding='utf-8') as f:
                    f.write(f"{i+1}")
                print(f"已保存进度: {i+1}/{total_chars}")
        except Exception as e:
            print(f"生成字符 '{char}' 时出错: {e}")
    
    print(f"完成! 已生成 {len(generated_files)} 个字符位图文件到 {output_dir}")
    return generated_files

def main():
    parser = argparse.ArgumentParser(description='从系统字体生成位图字符文件')
    parser.add_argument('font_path', help='字体文件路径')
    parser.add_argument('output_dir', help='输出目录')
    parser.add_argument('--chars', help='要生成的字符 (如不提供，将使用CP932字符集)')
    parser.add_argument('--size', type=int, default=16, help='字符高度 (默认: 16)')
    parser.add_argument('--char-file', help='从文件读取字符集 (每行一个字符)')
    parser.add_argument('--batch-size', type=int, default=100, help='每批处理的字符数量 (默认: 100)')
    parser.add_argument('--resume', action='store_true', help='从上次中断的位置继续')
    
    args = parser.parse_args()
    
    # 确定字符集
    char_set = None
    if args.char_file:
        with open(args.char_file, 'r', encoding='utf-8') as f:
            char_set = f.read().replace('\n', '')
    elif args.chars:
        char_set = args.chars
    
    # 确定起始索引
    start_index = 0
    if args.resume:
        progress_file = os.path.join(args.output_dir, "progress.txt")
        if os.path.exists(progress_file):
            with open(progress_file, 'r', encoding='utf-8') as f:
                try:
                    start_index = int(f.read().strip())
                    print(f"从索引 {start_index} 继续处理")
                except ValueError:
                    print("无法读取进度文件，从头开始")
    
    generate_character_set(args.font_path, args.output_dir, char_set, args.size, args.batch_size, start_index)

if __name__ == "__main__":
    main()
