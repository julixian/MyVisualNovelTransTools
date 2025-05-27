#!/usr/bin/env python3
import os
import struct
import argparse
from typing import List, Dict, Tuple, BinaryIO, Optional

class CODDecoder:
    """COD 文件解码器，用于解压和解密 COD 脚本文件"""
    
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
    
    def log(self, message: str):
        """打印调试信息（如果启用了详细模式）"""
        if self.verbose:
            print(message)
    
    def read_cod_file(self, filepath: str) -> Dict:
        """读取并解析 COD 文件"""
        with open(filepath, 'rb') as f:
            # 检查文件头
            header = f.read(3)
            if header != b'COD':
                raise ValueError(f"无效的 COD 文件: {filepath}，文件头应为 'COD'，实际为 {header}")
            
            self.log(f"有效的 COD 文件头: {header.decode('ascii')}")
            
            # 读取并解压四个数据块
            blocks = []
            for i in range(4):
                self.log(f"解压数据块 {i+1}...")
                block_data = self.decompress_block(f)
                if block_data:
                    # 解密数据（按位取反）
                    decrypted_data = bytes([~b & 0xFF for b in block_data])
                    blocks.append(decrypted_data)
                    self.log(f"数据块 {i+1} 解压后大小: {len(decrypted_data)} 字节")
                else:
                    self.log(f"数据块 {i+1} 解压失败")
                    blocks.append(None)
            
            # 解析数据块
            result = {
                'script_code': blocks[0],
                'functions': self.parse_function_table(blocks[1]) if blocks[1] else None,
                'variables': self.parse_variable_table(blocks[2]) if blocks[2] else None,
                'resources': self.parse_resource_table(blocks[3]) if blocks[3] else None
            }
            
            return result
    
    def decompress_block(self, f: BinaryIO) -> Optional[bytes]:
        """解压单个数据块"""
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
    
    def lz77_decompress(self, bitmap: bytes, offset_length_data: bytes, 
                        operations_count: int, uncompressed_size: int) -> bytes:
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
    
    def parse_function_table(self, data: bytes) -> List[Dict]:
        """解析函数表数据块"""
        functions = []
        if len(data) < 4:
            return functions
            
        count = struct.unpack('<I', data[0:4])[0]
        self.log(f"函数数量: {count}")
        
        offset = 4
        for i in range(count):
            if offset + 36 > len(data):
                break
                
            # 读取函数名（以 null 结尾的字符串）
            name_end = data.find(b'\0', offset)
            if name_end == -1 or name_end > offset + 32:
                name_end = offset + 32
                
            name = data[offset:name_end].decode('ascii', errors='replace')
            
            # 读取函数数据
            func_data = struct.unpack('<I', data[offset+32:offset+36])[0]
            
            functions.append({
                'name': name,
                'data': func_data
            })
            
            offset += 36
        
        return functions
    
    def parse_variable_table(self, data: bytes) -> List[Dict]:
        """解析变量表数据块"""
        variables = []
        if len(data) < 4:
            return variables
            
        count = struct.unpack('<I', data[0:4])[0]
        self.log(f"变量数量: {count}")
        
        offset = 4
        for i in range(count):
            if offset + 34 > len(data):
                break
                
            # 读取变量名（以 null 结尾的字符串）
            name_end = data.find(b'\0', offset)
            if name_end == -1 or name_end > offset + 32:
                name_end = offset + 32
                
            name = data[offset:name_end].decode('ascii', errors='replace')
            
            # 读取变量类型/值
            var_data = struct.unpack('<H', data[offset+32:offset+34])[0]
            
            variables.append({
                'name': name,
                'type_value': var_data
            })
            
            offset += 34
        
        return variables
    
    def parse_resource_table(self, data: bytes) -> List[Dict]:
        """解析资源表数据块"""
        resources = []
        if len(data) < 4:
            return resources
            
        count = struct.unpack('<I', data[0:4])[0]
        self.log(f"资源数量: {count}")
        
        offset = 4
        for i in range(count):
            if offset + 36 > len(data):
                break
                
            # 读取资源名（以 null 结尾的字符串）
            name_end = data.find(b'\0', offset)
            if name_end == -1 or name_end > offset + 32:
                name_end = offset + 32
                
            name = data[offset:name_end].decode('ascii', errors='replace')
            
            # 读取资源数据
            res_data = struct.unpack('<I', data[offset+32:offset+36])[0]
            
            resources.append({
                'name': name,
                'data': res_data
            })
            
            offset += 36
        
        return resources
    
    def save_decoded_data(self, data: Dict, output_dir: str):
        """将解码后的数据保存到文件"""
        os.makedirs(output_dir, exist_ok=True)
        
        # 保存脚本代码
        if data['script_code']:
            with open(os.path.join(output_dir, 'script_code.bin'), 'wb') as f:
                f.write(data['script_code'])
            self.log(f"脚本代码已保存到 {os.path.join(output_dir, 'script_code.bin')}")
        
        # 保存函数表
        if data['functions']:
            with open(os.path.join(output_dir, 'functions.txt'), 'w', encoding='utf-8') as f:
                f.write(f"函数数量: {len(data['functions'])}\n\n")
                for i, func in enumerate(data['functions']):
                    f.write(f"[{i}] 名称: {func['name']}, 数据: {func['data']}\n")
            self.log(f"函数表已保存到 {os.path.join(output_dir, 'functions.txt')}")
        
        # 保存变量表
        if data['variables']:
            with open(os.path.join(output_dir, 'variables.txt'), 'w', encoding='utf-8') as f:
                f.write(f"变量数量: {len(data['variables'])}\n\n")
                for i, var in enumerate(data['variables']):
                    f.write(f"[{i}] 名称: {var['name']}, 类型/值: {var['type_value']}\n")
            self.log(f"变量表已保存到 {os.path.join(output_dir, 'variables.txt')}")
        
        # 保存资源表
        if data['resources']:
            with open(os.path.join(output_dir, 'resources.txt'), 'w', encoding='utf-8') as f:
                f.write(f"资源数量: {len(data['resources'])}\n\n")
                for i, res in enumerate(data['resources']):
                    f.write(f"[{i}] 名称: {res['name']}, 数据: {res['data']}\n")
            self.log(f"资源表已保存到 {os.path.join(output_dir, 'resources.txt')}")

def main():
    parser = argparse.ArgumentParser(description='解压并解密 COD 脚本文件')
    parser.add_argument('input_file', help='输入的 COD 文件路径')
    parser.add_argument('output_dir', help='输出目录')
    parser.add_argument('-v', '--verbose', action='store_true', help='启用详细输出')
    
    args = parser.parse_args()
    
    decoder = CODDecoder(verbose=args.verbose)
    
    try:
        print(f"开始处理 COD 文件: {args.input_file}")
        decoded_data = decoder.read_cod_file(args.input_file)
        decoder.save_decoded_data(decoded_data, args.output_dir)
        print(f"COD 文件解压并解密完成，结果保存在: {args.output_dir}")
    except Exception as e:
        print(f"处理 COD 文件时出错: {e}")

if __name__ == "__main__":
    main()
