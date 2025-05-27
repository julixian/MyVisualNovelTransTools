import struct
import os
import argparse
from PIL import Image
import numpy as np
from io import BytesIO

class PNG2GR2Converter:
    def __init__(self, input_file):
        """初始化转换器"""
        self.input_file = input_file
        self.image = None
        self.width = 0
        self.height = 0
        self.bpp = 16  # 固定为16位RGB565
        self.version = 2  # 固定为GR2格式
        self.raw_data = None
        self.compressed_data = None
        self.control_bits = []
        
        # 与原始代码保持一致的参数
        self.count_bits = 5  # GR2格式使用5位表示长度
        self.count_mask = 0x1F  # 二进制: 00011111
        self.window_size = 2048  # 2^11，由偏移量位数决定
        self.max_match = 32  # 最大匹配长度 (count_mask + 1)
    
    def load_image(self):
        """加载PNG图像并转换为适当的格式"""
        try:
            self.image = Image.open(self.input_file)
            self.width, self.height = self.image.size
            
            # 确保图像是RGB模式
            if self.image.mode != 'RGB':
                self.image = self.image.convert('RGB')
            
            print(f"图像信息: {self.width}x{self.height}, 将转换为16位RGB565")
            return True
        except Exception as e:
            print(f"加载图像失败: {str(e)}")
            return False
    
    def convert_to_rgb565(self):
        """将图像转换为RGB565格式的原始数据"""
        if self.image is None:
            raise ValueError("没有加载图像")
        
        # 创建缓冲区存储RGB565数据
        self.raw_data = bytearray(self.width * self.height * 2)  # 16位 = 2字节/像素
        
        # 获取像素数据
        pixels = self.image.load()
        
        # 转换为RGB565
        for y in range(self.height):
            for x in range(self.width):
                r, g, b = pixels[x, y]
                
                # 转换为RGB565格式 (5位R, 6位G, 5位B)
                r5 = (r >> 3) & 0x1F
                g6 = (g >> 2) & 0x3F
                b5 = (b >> 3) & 0x1F
                
                # 组合为16位值 (RGB565)
                rgb565 = (r5 << 11) | (g6 << 5) | b5
                
                # 存储为小端序
                idx = (y * self.width + x) * 2
                self.raw_data[idx] = rgb565 & 0xFF
                self.raw_data[idx + 1] = (rgb565 >> 8) & 0xFF
        
        return True
    
    def compress_lz77(self):
        """使用LZ77算法压缩RGB565数据，使用与原始代码一致的参数"""
        if self.raw_data is None:
            raise ValueError("没有原始数据可压缩")
        
        input_data = self.raw_data
        output_data = bytearray()
        self.control_bits = []
        
        pos = 0
        data_len = len(input_data)
        
        # 最小匹配长度，通常为3（小于这个值不值得使用引用）
        min_match = 3
        
        while pos < data_len:
            # 查找最佳匹配
            best_length = 0
            best_offset = 0
            
            # 限制回溯窗口大小为2048（与原始代码一致）
            search_start = max(0, pos - self.window_size)
            
            # 尝试找到最长匹配
            if pos < data_len - min_match + 1:
                for offset in range(1, pos - search_start + 1):
                    # 计算可能的匹配长度
                    match_pos = pos - offset
                    match_len = 0
                    
                    while (pos + match_len < data_len and 
                           match_len < self.max_match and 
                           input_data[match_pos + match_len] == input_data[pos + match_len]):
                        match_len += 1
                    
                    # 如果找到更好的匹配
                    if match_len >= min_match and match_len > best_length:
                        best_length = match_len
                        best_offset = offset
            
            # 输出匹配或字面量
            if best_length >= min_match:
                # 确保长度不超过可表示的最大值
                if best_length > self.count_mask + 1:
                    best_length = self.count_mask + 1
                
                # 编码为引用 (5位长度，11位偏移)
                offset_length = ((best_offset - 1) << self.count_bits) | ((best_length - 1) & self.count_mask)
                output_data.extend(struct.pack('<H', offset_length))
                
                # 添加控制位 1 (引用)
                self.control_bits.append(1)
                
                pos += best_length
            else:
                # 直接输出字节
                output_data.append(input_data[pos])
                
                # 添加控制位 0 (字面量)
                self.control_bits.append(0)
                
                pos += 1
        
        self.compressed_data = output_data
        return True
    
    def create_gr2_file(self, output_file):
        """创建GR2格式文件"""
        if self.compressed_data is None or not self.control_bits:
            raise ValueError("没有压缩数据")
        
        with open(output_file, 'wb') as f:
            # 写入文件头
            f.write(b'GR')                                    # 标识符
            f.write(bytes([ord('0') + self.version]))         # 版本号
            f.write(struct.pack('<H', self.bpp))              # 色深
            f.write(struct.pack('<I', self.width))            # 宽度
            f.write(struct.pack('<I', self.height))           # 高度
            f.write(struct.pack('<I', len(self.raw_data)))    # 解压后大小
            
            # 写入控制位流长度 (位数)
            f.write(struct.pack('<I', len(self.control_bits)))
            
            # 写入控制位流数据
            control_bytes = bytearray((len(self.control_bits) + 7) // 8)
            for i, bit in enumerate(self.control_bits):
                if bit:
                    byte_idx = i // 8
                    bit_idx = i % 8
                    control_bytes[byte_idx] |= (1 << bit_idx)
            
            f.write(control_bytes)
            
            # 写入未知的4字节 (填充为0)
            f.write(struct.pack('<I', 0))
            
            # 写入压缩数据
            f.write(self.compressed_data)
            
            print(f"GR2文件已创建: {output_file}")
            print(f"原始大小: {len(self.raw_data)} 字节")
            compressed_size = len(self.compressed_data) + len(control_bytes) + 4
            print(f"压缩后大小: {compressed_size} 字节")
            print(f"压缩率: {(1 - compressed_size / len(self.raw_data)) * 100:.2f}%")
            
            return True
    
    def convert(self, output_file=None):
        """将PNG转换为GR2"""
        try:
            # 如果没有指定输出文件名，则使用输入文件名
            if output_file is None:
                base_name = os.path.splitext(self.input_file)[0]
                output_file = f"{base_name}.grp"
            
            # 加载图像
            if not self.load_image():
                return False
            
            # 转换为RGB565格式
            self.convert_to_rgb565()
            
            # 压缩数据
            self.compress_lz77()
            
            # 创建GR2文件
            self.create_gr2_file(output_file)
            
            return True
            
        except Exception as e:
            print(f"转换失败: {str(e)}")
            import traceback
            traceback.print_exc()
            return False


def main():
    parser = argparse.ArgumentParser(description='将PNG图像转换为GR2格式(16位RGB565)')
    parser.add_argument('input', help='输入PNG文件路径')
    parser.add_argument('-o', '--output', help='输出GR2文件路径 (默认为输入文件名.grp)')
    parser.add_argument('--no-compress', action='store_true', help='不进行压缩，直接存储数据')
    
    args = parser.parse_args()
    
    converter = PNG2GR2Converter(args.input)
    converter.convert(args.output)


if __name__ == "__main__":
    main()
