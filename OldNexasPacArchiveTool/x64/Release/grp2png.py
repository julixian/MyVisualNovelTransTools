import struct
import os
import argparse
from PIL import Image
import numpy as np
from io import BytesIO

class GR2Converter:
    def __init__(self, input_file):
        """初始化转换器"""
        self.input_file = input_file
        self.width = 0
        self.height = 0
        self.bpp = 0
        self.version = 0
        self.unpacked_size = 0
        self.palette = None
        self.data = None
    
    def read_header(self):
        """读取GR2文件头"""
        with open(self.input_file, 'rb') as f:
            header = f.read(0x11)
            
            # 检查文件标识
            if header[0:2] != b'GR':
                raise ValueError("不是有效的GR文件")
            
            # 检查版本
            self.version = header[2] - ord('0')
            if self.version < 1 or self.version > 3:
                raise ValueError(f"不支持的GR版本: {self.version}")
            
            # 读取基本信息
            self.bpp = struct.unpack('<H', header[3:5])[0]
            self.width = struct.unpack('<I', header[5:9])[0]
            self.height = struct.unpack('<I', header[9:13])[0]
            
            # 读取或计算解压后的大小
            if self.version > 1:
                self.unpacked_size = struct.unpack('<I', header[13:17])[0]
            else:
                bytes_per_pixel = self.bpp // 8
                self.unpacked_size = self.width * self.height * bytes_per_pixel
                if self.bpp == 8:
                    self.unpacked_size += 0x300  # 调色板大小
            
            print(f"图像信息: {self.width}x{self.height}, {self.bpp}位, 版本GR{self.version}")
            print(f"解压后大小: {self.unpacked_size} 字节")
            
            return True
    
    def decompress(self):
        """解压GR2/GR3文件数据"""
        with open(self.input_file, 'rb') as f:
            # 跳过文件头
            f.seek(0x11)
            
            if self.version < 2:
                # 版本1不使用压缩
                f.seek(0x0D)  # 回到数据开始位置
                self.data = f.read(self.unpacked_size)
                return
            
            # 读取控制位流信息
            ctl_bits_length = struct.unpack('<I', f.read(4))[0]
            ctl_bytes_length = (ctl_bits_length + 7) // 8
            ctl_bytes = f.read(ctl_bytes_length)
            
            # 跳过未使用的4字节
            f.read(4)
            
            # 准备解压缩
            self.data = bytearray(self.unpacked_size)
            dst = 0
            
            # 设置count_bits (GR3使用3位，GR2使用5位)
            count_bits = 3 if self.version > 2 else 5
            count_mask = (1 << count_bits) - 1
            
            # 处理控制位流
            bit_pos = 0
            for bit_index in range(ctl_bits_length):
                # 获取下一个控制位
                byte_index = bit_index // 8
                bit_offset = bit_index % 8
                if byte_index >= len(ctl_bytes):
                    break
                    
                bit = (ctl_bytes[byte_index] >> bit_offset) & 1
                
                if bit == 0:
                    # 直接复制一个字节
                    if dst >= self.unpacked_size:
                        break
                    self.data[dst] = f.read(1)[0]
                    dst += 1
                else:
                    # 引用之前的数据
                    offset_length = struct.unpack('<H', f.read(2))[0]
                    count = (offset_length & count_mask) + 1
                    offset = (offset_length >> count_bits) + 1
                    
                    # 使用重叠复制
                    for i in range(count):
                        if dst >= self.unpacked_size:
                            break
                        self.data[dst] = self.data[dst - offset]
                        dst += 1
    
    def convert_to_image(self):
        """将解压后的数据转换为PIL图像"""
        if self.data is None:
            raise ValueError("没有可用的图像数据")
        
        if self.bpp == 8:
            # 8位索引色图像
            # 提取调色板 (位于数据末尾)
            palette_data = self.data[-0x300:]
            palette = []
            for i in range(0, 0x300, 3):
                r, g, b = palette_data[i:i+3]
                palette.extend((r, g, b))
            
            # 创建索引色图像
            img_data = self.data[:-0x300]
            img = Image.frombytes('P', (self.width, self.height), bytes(img_data))
            img.putpalette(palette)
            
        elif self.bpp == 16:
            # 16位图像 (使用RGB565而不是RGB555)
            img_array = np.zeros((self.height, self.width, 3), dtype=np.uint8)
            
            for y in range(self.height):
                for x in range(self.width):
                    idx = (y * self.width + x) * 2
                    if idx + 1 >= len(self.data):
                        break
                        
                    pixel = struct.unpack('<H', self.data[idx:idx+2])[0]
                    
                    # RGB565解码
                    r = ((pixel >> 11) & 0x1F) << 3
                    g = ((pixel >> 5) & 0x3F) << 2
                    b = (pixel & 0x1F) << 3
                    
                    # 填充最低位以获得更准确的颜色
                    r |= r >> 5
                    g |= g >> 6
                    b |= b >> 5
                    
                    img_array[y, x] = [r, g, b]
            
            img = Image.fromarray(img_array, 'RGB')
            
        elif self.bpp == 24:
            # 24位BGR图像
            img_array = np.zeros((self.height, self.width, 3), dtype=np.uint8)
            
            for y in range(self.height):
                for x in range(self.width):
                    idx = (y * self.width + x) * 3
                    if idx + 2 >= len(self.data):
                        break
                        
                    b, g, r = self.data[idx:idx+3]
                    img_array[y, x] = [r, g, b]
            
            img = Image.fromarray(img_array, 'RGB')
            
        elif self.bpp == 32:
            # 32位BGRA图像
            img_array = np.zeros((self.height, self.width, 4), dtype=np.uint8)
            
            for y in range(self.height):
                for x in range(self.width):
                    idx = (y * self.width + x) * 4
                    if idx + 3 >= len(self.data):
                        break
                        
                    b, g, r, a = self.data[idx:idx+4]
                    img_array[y, x] = [r, g, b, a]
            
            img = Image.fromarray(img_array, 'RGBA')
            
        else:
            raise ValueError(f"不支持的色深: {self.bpp}位")
        
        return img
    
    def convert(self, output_file=None):
        """将GR2文件转换为PNG"""
        try:
            # 读取文件头
            self.read_header()
            
            # 解压数据
            self.decompress()
            
            # 转换为图像
            img = self.convert_to_image()
            
            # 如果没有指定输出文件名，则使用输入文件名
            if output_file is None:
                base_name = os.path.splitext(self.input_file)[0]
                output_file = f"{base_name}.png"
            
            # 保存为PNG
            img.save(output_file, "PNG")
            print(f"已成功转换为: {output_file}")
            return True
            
        except Exception as e:
            print(f"转换失败: {str(e)}")
            return False


def main():
    parser = argparse.ArgumentParser(description='将GR2/GR3图像转换为PNG')
    parser.add_argument('input', help='输入GR2/GR3文件路径')
    parser.add_argument('-o', '--output', help='输出PNG文件路径 (默认为输入文件名.png)')
    
    args = parser.parse_args()
    
    converter = GR2Converter(args.input)
    converter.convert(args.output)


if __name__ == "__main__":
    main()
