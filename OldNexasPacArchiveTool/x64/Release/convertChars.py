import numpy as np
from PIL import Image
import sys
import argparse
import os
from pathlib import Path

def convert_font_bitmap_chunk_to_png(data, output_path, index, width=16, height=16, bits_per_pixel=2):
    """
    将一块字体位图数据转换为PNG图像
    
    参数:
    data -- 位图数据字节
    output_path -- 输出文件夹路径
    index -- 当前字符索引(用于文件命名)
    width -- 字符宽度 (默认 16)
    height -- 字符高度 (默认 16)
    bits_per_pixel -- 每像素的位数 (默认 2)
    """
    # 创建图像数组 (灰度图)
    image = np.zeros((height, width), dtype=np.uint8)
    
    # 解析每个像素
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
    
    # 创建文件名
    filename = f"{index:04d}.png"
    output_file = os.path.join(output_path, filename)
    
    # 创建PIL图像并保存
    img = Image.fromarray(image)
    img.save(output_file)
    
    # 可选：创建放大版本
    enlarged_path = os.path.join(output_path, "enlarged")
    if not os.path.exists(enlarged_path):
        os.makedirs(enlarged_path)
    enlarged = img.resize((width*4, height*4), Image.NEAREST)
    enlarged.save(os.path.join(enlarged_path, filename))
    
    return filename

def batch_convert_font_bitmap_to_pngs(input_file, output_folder, width=16, height=16, bits_per_pixel=2):
    """
    将大型字体位图文件批量转换为多个PNG图像
    
    参数:
    input_file -- 输入的位图文件路径
    output_folder -- 输出的PNG文件夹路径
    width -- 字符宽度 (默认 16)
    height -- 字符高度 (默认 16)
    bits_per_pixel -- 每像素的位数 (默认 2)
    """
    # 确保输出文件夹存在
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)
    
    # 计算每个字符需要的字节数
    chunk_size = (width * height * bits_per_pixel) // 8
    
    # 打开文件并逐块读取处理
    with open(input_file, 'rb') as f:
        index = 0
        while True:
            data = f.read(chunk_size)
            if not data:  # 如果没有更多数据，退出循环
                break
                
            # 如果读取的数据不足一个完整字符，可能是文件末尾
            if len(data) < chunk_size:
                print(f"警告: 最后一块数据大小 ({len(data)} 字节) 小于预期 ({chunk_size} 字节)")
                # 可以选择跳过这一块，或者用零填充
                if len(data) < chunk_size // 2:  # 如果太小，就跳过
                    break
                # 否则填充到完整大小
                data = data + bytes(chunk_size - len(data))
            
            # 转换当前块为PNG
            filename = convert_font_bitmap_chunk_to_png(data, output_folder, index, width, height, bits_per_pixel)
            
            # 每处理10个字符输出一次进度
            if index % 10 == 0:
                print(f"已处理 {index} 个字符，当前: {filename}")
                
            index += 1
    
    print(f"转换完成！共处理了 {index} 个字符")
    print(f"PNG文件保存在: {output_folder}")
    print(f"放大版本保存在: {os.path.join(output_folder, 'enlarged')}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='将字体位图文件批量转换为PNG图像')
    parser.add_argument('input_file', help='输入的位图文件路径')
    parser.add_argument('output_folder', help='输出的PNG文件夹路径')
    parser.add_argument('--width', type=int, default=16, help='字符宽度 (默认: 16)')
    parser.add_argument('--height', type=int, default=16, help='字符高度 (默认: 16)')
    parser.add_argument('--bits', type=int, default=2, help='每像素的位数 (默认: 2)')
    
    args = parser.parse_args()
    
    batch_convert_font_bitmap_to_pngs(args.input_file, args.output_folder, 
                                     args.width, args.height, args.bits)
