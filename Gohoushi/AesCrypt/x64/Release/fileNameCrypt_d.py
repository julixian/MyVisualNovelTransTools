#!/usr/bin/env python3
import os
import sys
import shutil
import string
import argparse

def obfuscate_name(base_str):
    """
    实现与游戏中相同的文件名混淆算法
    """
    if len(base_str) >= 13:
        return None
    
    # 计算字符和的模26作为偏移量
    offset = sum(ord(c) for c in base_str) % 26
    
    result = []
    for i, c in enumerate(base_str):
        is_odd_position = (i & 1) != 0
        
        if c >= '0' and c <= '9':
            if is_odd_position:
                j = ord(c) + ord('a') - ord('0')
            else:
                j = ord('z') - (ord(c) - ord('0'))
        elif c >= 'a' and c <= 'z':
            if is_odd_position:
                j = ord('z') - ord(c) + ord('a')
            else:
                j = ord(c)
        elif c == '_':
            j = ord('a')
        else:
            return None
        
        # 应用偏移量
        j += offset
        while j > ord('z'):
            j -= 25
        
        result.append(chr(j))
    
    return ''.join(result)

def find_and_copy_files(source_dir, target_dir, mapping_file):
    """
    尝试各种可能的原始文件名，找到混淆后的文件并复制
    """
    if not os.path.exists(source_dir):
        print(f"错误: 源目录 '{source_dir}' 不存在")
        return
    
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
    
    # 获取源目录中的所有文件
    source_files = set(os.listdir(source_dir))
    
    # 创建映射文件
    with open(mapping_file, 'w', encoding='utf-8') as f:
        f.write("原始文件名,混淆后文件名\n")
    
    found_count = 0
    
    # 生成可能的原始文件名并尝试
    for letter in string.ascii_lowercase:  # a-z
        for digit1 in range(10):  # 0-9
            for digit2 in range(10):
                original_name = f"gh_{letter}{digit1}{digit2}"
                obfuscated_name = obfuscate_name(original_name)
                
                if obfuscated_name and obfuscated_name in source_files:
                    found_count += 1
                    
                    # 记录映射关系
                    with open(mapping_file, 'a', encoding='utf-8') as f:
                        f.write(f"{original_name},{obfuscated_name}\n")
                    
                    # 复制文件
                    source_path = os.path.join(source_dir, obfuscated_name)
                    target_path = os.path.join(target_dir, original_name)
                    
                    try:
                        shutil.copy2(source_path, target_path)
                        print(f"已复制: {obfuscated_name} -> {original_name}")
                    except Exception as e:
                        print(f"复制文件时出错: {e}")
    
    print(f"完成! 找到并处理了 {found_count} 个文件。")
    print(f"映射关系已保存到 '{mapping_file}'")

def main():
    parser = argparse.ArgumentParser(description='还原游戏文件名混淆')
    parser.add_argument('source_dir', help='包含混淆文件名文件的源目录')
    parser.add_argument('target_dir', help='存放还原后文件的目标目录')
    parser.add_argument('--map', default='filename_mapping.csv', help='保存文件名映射关系的文件 (默认: filename_mapping.csv)')
    
    args = parser.parse_args()
    
    find_and_copy_files(args.source_dir, args.target_dir, args.map)

if __name__ == "__main__":
    main()
