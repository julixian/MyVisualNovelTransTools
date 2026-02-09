# 替换表配置
replace_dict = {
    r"\\n　": "\\n",
}

# 目录配置
input_directory = "new_txt"  # 替换为你的实际输入目录
output_directory = "output"  # 替换为你的实际输出目录

# 编码配置
default_encoding = 'auto'  # 可选 'auto' 或具体编码如 'utf-16'

import os
import codecs

def detect_encoding(file_path, default_encoding='utf-16'):
    """
    尝试检测文件编码，如果无法检测则返回默认编码
    """
    encodings = ['utf-8', 'utf-16', 'gb18030']
    for enc in encodings:
        try:
            with codecs.open(file_path, 'r', encoding=enc) as file:
                file.read()
            return enc
        except UnicodeDecodeError:
            continue
    return default_encoding

def replace_text_in_files(input_directory, output_directory, replace_dict, encoding='auto'):
    for root, dirs, files in os.walk(input_directory):
        for filename in files:
            if filename.endswith(".txt"):
                input_file_path = os.path.join(root, filename)
                
                # 创建相对路径
                relative_path = os.path.relpath(root, input_directory)
                output_subdir = os.path.join(output_directory, relative_path)
                
                # 确保输出子目录存在
                os.makedirs(output_subdir, exist_ok=True)
                
                output_file_path = os.path.join(output_subdir, filename)
                
                # 检测或使用指定的编码
                file_encoding = detect_encoding(input_file_path) if encoding == 'auto' else encoding
                
                with codecs.open(input_file_path, "r", encoding=file_encoding) as file:
                    text = file.read()
                
                modified_text = replace_text(text, replace_dict)
                with codecs.open(output_file_path, "w", encoding=file_encoding) as file:
                    file.write(modified_text)
                
                print(f"处理文件: {input_file_path} -> {output_file_path} (编码: {file_encoding})")

def replace_text(input_text, replace_dict):
    lines = input_text.split("\n")
    modified_lines = []
    for line in lines:
        if line.startswith("★"):
            modified_line = line
            for old, new in replace_dict.items():
                modified_line = modified_line.replace(old, new)
            modified_lines.append(modified_line)
        else:
            modified_lines.append(line)
    return "\n".join(modified_lines)

# 执行替换
replace_text_in_files(input_directory, output_directory, replace_dict, encoding=default_encoding)
