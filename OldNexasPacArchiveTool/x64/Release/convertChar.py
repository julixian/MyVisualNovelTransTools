import numpy as np
from PIL import Image
import sys
import argparse

def convert_font_bitmap_to_png(input_file, output_file, width=16, height=16, bits_per_pixel=2):
    """
    将字体位图文件转换为PNG图像
    
    参数:
    input_file -- 输入的位图文件路径
    output_file -- 输出的PNG文件路径
    width -- 字符宽度 (默认 16)
    height -- 字符高度 (默认 16)
    bits_per_pixel -- 每像素的位数 (默认 2)
    """
    # 读取二进制文件
    with open(input_file, 'rb') as f:
        data = f.read()
    
    # 检查文件大小是否符合预期
    expected_size = (width * height * bits_per_pixel) // 8
    if len(data) != expected_size:
        print(f"警告: 文件大小 ({len(data)} 字节) 与预期大小 ({expected_size} 字节) 不符")
    
    # 创建图像数组 (灰度图)
    image = np.zeros((height, width), dtype=np.uint8)
    
    # 解析每个像素
    byte_index = 0
    bit_index = 0
    
    for y in range(height):
        for x in range(width):
            # 计算字节索引和位索引
            byte_index = (y * width * bits_per_pixel + x * bits_per_pixel) // 8
            bit_index = (y * width * bits_per_pixel + x * bits_per_pixel) % 8
            
            # 如果超出数据范围，则跳过
            if byte_index >= len(data):
                continue
            
            # 从字节中提取像素值 (2位)
            pixel_value = (data[byte_index] >> bit_index) & 0x3
            
            # 将2位值 (0-3) 转换为8位灰度值 (0-255)
            # 0 = 透明/白色 (255), 3 = 不透明/黑色 (0)
            if pixel_value == 0:
                gray_value = 255  # 白色/透明
            else:
                # 将1-3映射为灰度值
                gray_value = 255 - (pixel_value * 85)  # 170, 85, 0
            
            # 存储到图像数组
            image[y, x] = gray_value
    
    # 创建PIL图像并保存
    img = Image.fromarray(image)
    img.save(output_file)
    print(f"成功将 {input_file} 转换为 {output_file}")
    print(f"图像尺寸: {width}x{height} 像素")
    
    # 显示更大版本的图像 (放大4倍以便于查看)
    enlarged = img.resize((width*4, height*4), Image.NEAREST)
    enlarged.save(output_file.replace('.png', '_enlarged.png'))
    print(f"已创建放大版本: {output_file.replace('.png', '_enlarged.png')}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='将字体位图文件转换为PNG图像')
    parser.add_argument('input_file', help='输入的位图文件路径')
    parser.add_argument('output_file', help='输出的PNG文件路径')
    parser.add_argument('--width', type=int, default=16, help='字符宽度 (默认: 16)')
    parser.add_argument('--height', type=int, default=16, help='字符高度 (默认: 16)')
    parser.add_argument('--bits', type=int, default=2, help='每像素的位数 (默认: 2)')
    
    args = parser.parse_args()
    
    convert_font_bitmap_to_png(args.input_file, args.output_file, 
                              args.width, args.height, args.bits)
