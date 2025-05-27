#!/usr/bin/env python3

import os
import sys
import argparse

def obfuscate_name(base_str):
    """
    实现与游戏中 BundleNameObfuscator.Obfuscate 方法相同的混淆算法
    """
    if len(base_str) >= 13:
        print(f"Error: String '{base_str}' is too long (max 12 characters)")
        return None
    
    # 计算字符和的模26值作为偏移量
    num = 0
    for c in base_str:
        num += ord(c)
    num %= 26
    
    result = ""
    for i, c in enumerate(base_str):
        is_odd = (i & 1) != 0
        
        if '0' <= c <= '9':
            if is_odd:
                j = ord(c) - ord('0') + ord('a')
            else:
                j = ord('z') - (ord(c) - ord('0'))
        elif 'a' <= c <= 'z':
            if is_odd:
                j = ord('z') - ord(c) + ord('a')
            else:
                j = ord(c)
        elif c == '_':
            j = 97  # 'a'
        else:
            print(f"Error: Invalid character '{c}' in string '{base_str}'")
            return None
        
        # 应用偏移量
        j += num
        while j > 122:  # 'z'
            j -= 25
        
        result += chr(j)
    
    return result

def rename_files_in_directory(directory, dry_run=False, verbose=False):
    """
    将指定目录中的所有文件重命名为其混淆后的名称
    """
    # 存储原始名称到混淆名称的映射
    name_mapping = {}
    
    # 首先计算所有文件的混淆名称
    for filename in os.listdir(directory):
        filepath = os.path.join(directory, filename)
        
        # 跳过目录
        if os.path.isdir(filepath):
            if verbose:
                print(f"Skipping directory: {filename}")
            continue
        
        # 获取文件名（不含扩展名）和扩展名
        name, ext = os.path.splitext(filename)
        
        # 混淆文件名
        obfuscated_name = obfuscate_name(name)
        if obfuscated_name is None:
            if verbose:
                print(f"Skipping file with invalid name: {filename}")
            continue
        
        # 构建新文件名（保留原始扩展名）
        new_filename = obfuscated_name + ext
        name_mapping[filename] = new_filename
        
        if verbose:
            print(f"Will rename: {filename} -> {new_filename}")
    
    # 检查是否有命名冲突
    if len(set(name_mapping.values())) != len(name_mapping):
        print("Error: Name collision detected after obfuscation!")
        value_counts = {}
        for value in name_mapping.values():
            value_counts[value] = value_counts.get(value, 0) + 1
        
        for value, count in value_counts.items():
            if count > 1:
                print(f"  {value} would be used for {count} different files:")
                for orig, new in name_mapping.items():
                    if new == value:
                        print(f"    - {orig}")
        return
    
    # 执行重命名操作
    if not dry_run:
        # 使用临时名称进行两阶段重命名，避免冲突
        temp_mappings = {}
        for i, (old_name, new_name) in enumerate(name_mapping.items()):
            temp_name = f"__temp__{i}{os.path.splitext(old_name)[1]}"
            temp_mappings[old_name] = (temp_name, new_name)
        
        # 第一阶段：重命名为临时名称
        for old_name, (temp_name, _) in temp_mappings.items():
            old_path = os.path.join(directory, old_name)
            temp_path = os.path.join(directory, temp_name)
            os.rename(old_path, temp_path)
            if verbose:
                print(f"Renamed: {old_name} -> {temp_name} (temporary)")
        
        # 第二阶段：重命名为最终名称
        for _, (temp_name, new_name) in temp_mappings.items():
            temp_path = os.path.join(directory, temp_name)
            new_path = os.path.join(directory, new_name)
            os.rename(temp_path, new_path)
            if verbose:
                print(f"Renamed: {temp_name} -> {new_name} (final)")
        
        print(f"Successfully renamed {len(name_mapping)} files.")
    else:
        print(f"Dry run: would rename {len(name_mapping)} files.")
    
    # 生成映射文件
    mapping_file = os.path.join(directory, "filename_mapping.txt")
    with open(mapping_file, "w") as f:
        f.write("Original Name,Obfuscated Name\n")
        for old_name, new_name in sorted(name_mapping.items()):
            f.write(f"{old_name},{new_name}\n")
    
    print(f"Mapping saved to: {mapping_file}")

def main():
    parser = argparse.ArgumentParser(description="Obfuscate filenames using the game's algorithm")
    parser.add_argument("directory", help="Directory containing files to rename")
    parser.add_argument("--dry-run", action="store_true", help="Show what would be renamed without actually renaming")
    parser.add_argument("--verbose", action="store_true", help="Show detailed information")
    parser.add_argument("--test", metavar="FILENAME", help="Test obfuscation on a single filename without renaming")
    
    args = parser.parse_args()
    
    if args.test:
        obfuscated = obfuscate_name(args.test)
        if obfuscated:
            print(f"Original: {args.test}")
            print(f"Obfuscated: {obfuscated}")
        return
    
    if not os.path.isdir(args.directory):
        print(f"Error: {args.directory} is not a valid directory")
        return
    
    rename_files_in_directory(args.directory, args.dry_run, args.verbose)

if __name__ == "__main__":
    main()
