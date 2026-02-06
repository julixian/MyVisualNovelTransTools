#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
字库映射替换和重新渲染工具 (修正版 v2)
- 基线对齐修复标点符号位置
- 字母数字左对齐修复偏右问题
"""

import struct
import os
import json
import unicodedata
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path


def load_char_mapping(txt_file):
    """从.txt文件加载字符映射表"""
    with open(txt_file, 'r', encoding='utf-8') as f:
        content = f.read()
        chars = content.split('\n')
        if chars and chars[-1] == '':
            chars = chars[:-1]
    print(f"从 {txt_file} 加载了 {len(chars)} 个字符")
    return chars


def load_substitution_rules(json_file):
    """从JSON文件加载替换规则"""
    with open(json_file, 'r', encoding='utf-8') as f:
        rules = json.load(f)
    print(f"从 {json_file} 加载了 {len(rules)} 条替换规则")
    return rules


def load_font_dat(dat_file):
    """加载.dat字库文件"""
    with open(dat_file, 'rb') as f:
        header = f.read(32)
        bitmap_width = struct.unpack('<H', header[0x10:0x12])[0]
        bitmap_height = struct.unpack('<H', header[0x12:0x14])[0]
        char_width = struct.unpack('<H', header[0x14:0x16])[0]
        char_height = struct.unpack('<H', header[0x16:0x18])[0]
        
        print(f"\n字库信息:")
        print(f"  位图尺寸: {bitmap_width} × {bitmap_height}")
        print(f"  字符尺寸: {char_width} × {char_height}")
        
        chars_per_row = bitmap_width // char_width
        total_chars = chars_per_row * (bitmap_height // char_height)
        
        bitmap_data = f.read(bitmap_width * bitmap_height)
        
        return {
            'header': header,
            'bitmap_width': bitmap_width,
            'bitmap_height': bitmap_height,
            'char_width': char_width,
            'char_height': char_height,
            'chars_per_row': chars_per_row,
            'total_chars': total_chars,
            'bitmap_data': bytearray(bitmap_data)
        }


def is_alphanumeric_char(char):
    """
    判断字符是否为字母或数字（包括全角和半角）
    这些字符需要左对齐而不是居中
    """
    # ASCII 半角字母数字 (A-Z, a-z, 0-9)
    if char.isalnum() and ord(char) < 128:
        return True
    
    code = ord(char)
    
    # 全角数字 ０-９ (U+FF10 - U+FF19)
    if 0xFF10 <= code <= 0xFF19:
        return True
    
    # 全角大写字母 Ａ-Ｚ (U+FF21 - U+FF3A)
    if 0xFF21 <= code <= 0xFF3A:
        return True
    
    # 全角小写字母 ａ-ｚ (U+FF41 - U+FF5A)
    if 0xFF41 <= code <= 0xFF5A:
        return True
    
    return False


def is_halfwidth_char(char):
    """判断是否为半角字符"""
    ea_width = unicodedata.east_asian_width(char)
    # Na=Narrow, H=Halfwidth, N=Neutral 都视为半角
    return ea_width in ['Na', 'H', 'N']


def render_char_with_font(char, font_path, width, height):
    """
    使用TTF字体渲染单个字符
    
    对齐规则：
    - 字母数字（全角/半角）：左对齐
    - 其他字符（汉字、标点等）：水平居中
    - 垂直方向：基线对齐
    """
    try:
        font_size = height  # 不缩小字体
        font = ImageFont.truetype(font_path, font_size)
        
        # 获取字体度量信息（用于基线对齐）
        try:
            ascent, descent = font.getmetrics()
        except:
            ascent = int(font_size * 0.8)
            descent = int(font_size * 0.2)
        
        font_total_height = ascent + descent
        
        # 创建画布
        img = Image.new('L', (width, height), color=0)
        draw = ImageDraw.Draw(img)
        
        # 获取字符墨水边界
        bbox = draw.textbbox((0, 0), char, font=font)
        text_width = bbox[2] - bbox[0]
        
        # === X 坐标计算 ===
        if is_alphanumeric_char(char):
            # 字母数字：左对齐，留 1-2 像素边距
            left_margin = 1
            x = left_margin - bbox[0]
        else:
            # 其他字符：水平居中
            x = (width - text_width) // 2 - bbox[0]
        
        # === Y 坐标计算 ===
        # 基线对齐：将整个字体行高居中，而不是单个字符的墨水居中
        y = (height - font_total_height) // 2
        
        # 绘制
        draw.text((x, y), char, fill=255, font=font)
        
        return img
        
    except Exception as e:
        return None


def check_font_has_char(char, font_path, height):
    """检查字体中是否存在某个字符"""
    try:
        font = ImageFont.truetype(font_path, height)
        draw = ImageDraw.Draw(Image.new('L', (10, 10)))
        bbox = draw.textbbox((0, 0), char, font=font)
        return bbox[2] > bbox[0] and bbox[3] > bbox[1]
    except:
        return False


def replace_char_in_dat(font_data, char_index, new_img):
    """将新渲染的字符写回位图数据"""
    row = char_index // font_data['chars_per_row']
    col = char_index % font_data['chars_per_row']
    
    cw = font_data['char_width']
    ch = font_data['char_height']
    bw = font_data['bitmap_width']
    bitmap = font_data['bitmap_data']
    
    for y in range(ch):
        for x in range(cw):
            offset = (row * ch + y) * bw + (col * cw + x)
            if offset < len(bitmap):
                bitmap[offset] = new_img.getpixel((x, y))


def save_font_dat(font_data, output_file):
    """保存修改后的.dat文件"""
    with open(output_file, 'wb') as f:
        f.write(font_data['header'])
        f.write(bytes(font_data['bitmap_data']))
    print(f"已保存到: {output_file}")


def process_font():
    """主处理流程"""
    current_dir = Path('.')
    
    txt_files = list(current_dir.glob('*.txt'))
    dat_files = list(current_dir.glob('*.dat'))
    json_file = current_dir / 'charMap.json'
    font_file = current_dir / 'julixiansimhei-Regular.ttf'
    
    if not txt_files or not dat_files or not json_file.exists() or not font_file.exists():
        print("错误: 缺少必要文件 (.txt, .dat, charMap.json, 或 .ttf)")
        return
    
    for txt_file in txt_files:
        dat_file = current_dir / (txt_file.stem + '.dat')
        if not dat_file.exists():
            continue
            
        print(f"\n{'='*60}")
        print(f"处理: {txt_file.name}")
        
        char_mapping = load_char_mapping(txt_file)
        sub_rules = load_substitution_rules(json_file)
        font_data = load_font_dat(dat_file)
        
        output_dir = Path('out')
        output_dir.mkdir(exist_ok=True)
        
        stats = {'replaced': 0, 'rendered': 0, 'kept': 0}
        total = min(len(char_mapping), font_data['total_chars'])
        
        for idx in range(total):
            original_char = char_mapping[idx]
            
            # 跳过空白
            if not original_char or original_char.strip() == '' or original_char == '�':
                stats['kept'] += 1
                continue
            
            # 查找替换
            target_char = original_char
            for cn, jp in sub_rules.items():
                if original_char == jp:
                    target_char = cn
                    stats['replaced'] += 1
                    break
            
            # 渲染
            if check_font_has_char(target_char, str(font_file), font_data['char_height']):
                new_img = render_char_with_font(
                    target_char, str(font_file),
                    font_data['char_width'], font_data['char_height']
                )
                if new_img:
                    replace_char_in_dat(font_data, idx, new_img)
                    stats['rendered'] += 1
                else:
                    stats['kept'] += 1
            else:
                stats['kept'] += 1
            
            if (idx + 1) % 100 == 0:
                print(f"进度: {idx + 1}/{total}", end='\r')
        
        print()
        
        # 保存
        save_font_dat(font_data, output_dir / dat_file.name)
        
        # 预览图
        preview = Image.new('L', (font_data['bitmap_width'], font_data['bitmap_height']))
        preview.putdata(font_data['bitmap_data'])
        preview.save(output_dir / f"{dat_file.stem}_preview.png")
        
        print(f"完成! 替换:{stats['replaced']} 渲染:{stats['rendered']} 保留:{stats['kept']}")


if __name__ == "__main__":
    process_font()
