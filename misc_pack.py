import os
import sys
import lzss_s
from pathlib import Path

class BinPacker:
    def __init__(self):
        self.file_count = 0
        self.indices = []  # [(offset, uncompressed_size), ...]
        self.compressed_data = []  # [(compressed_size, data), ...]

    def align_16(self, size):
        """将大小对齐到4字节"""
        if size % 4 != 0:
            size += 4 - (size % 4)
        return size

    def pack(self, input_dir, output_path):
        # 获取所有文件并排序
        files = sorted([f for f in Path(input_dir).glob('*') if f.is_file()])
        self.file_count = len(files)

        # 计算头部大小（文件数量4字节 + 索引表大小）
        header_size = 4 + self.file_count * 8
        current_offset = self.align_16(header_size)

        # 处理每个文件
        for file_path in files:
            print(f"Processing: {file_path.name}")
            
            # 读取文件数据
            with open(file_path, 'rb') as f:
                uncompressed_data = f.read()
            
            # 压缩数据
            # 为压缩结果预分配比原始数据大一些的空间
            compressed_buffer = bytearray(len(uncompressed_data) + 0x200)
            compressed_size = lzss_s.compress(compressed_buffer, uncompressed_data)
            compressed_data = compressed_buffer[:compressed_size]  # 截取实际使用的部分

            # 记录索引信息
            self.indices.append((current_offset, len(uncompressed_data)))
            self.compressed_data.append((compressed_size, compressed_data))

            # 更新偏移，确保16字节对齐
            current_offset = self.align_16(current_offset + 4 + compressed_size)

        # 写入文件
        with open(output_path, 'wb') as f:
            # 写入文件数量
            f.write(self.file_count.to_bytes(4, 'little'))

            # 写入索引表
            for offset, size in self.indices:
                f.write(offset.to_bytes(4, 'little'))
                f.write(size.to_bytes(4, 'little'))

            # 对齐到16字节
            current_pos = 4 + self.file_count * 8
            padding_size = self.align_16(current_pos) - current_pos
            f.write(b'\x00' * padding_size)

            # 写入压缩后的文件数据
            for compressed_size, data in self.compressed_data:
                # 写入压缩后大小
                f.write(compressed_size.to_bytes(4, 'little'))
                # 写入压缩数据
                f.write(data)
                # 16字节对齐
                current_pos = f.tell()
                padding_size = self.align_16(current_pos) - current_pos
                f.write(b'\x00' * padding_size)

def main():
    if len(sys.argv) != 3:
        print("Made by julixian 2025.03.12\nUsage: python programme.py <input_dir> <output_file>")
        return

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    if not os.path.isdir(input_path):
        print(f"Error：don't exist: {input_path}")
        return

    try:
        packer = BinPacker()
        packer.pack(input_path, output_path)
        print("Packing successfully！")
    except Exception as e:
        print(f"Fail to pack：{str(e)}")

if __name__ == '__main__':
    main()
