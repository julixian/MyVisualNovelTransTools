import struct
import os
from PIL import Image
from pathlib import Path

def sjis_to_unicode(sjis_code):
    """将Shift-JIS编码转换为Unicode字符"""
    try:
        # Shift-JIS编码转字节
        if sjis_code < 0x100:
            # 单字节字符
            bytes_data = bytes([sjis_code])
        else:
            # 双字节字符
            high = (sjis_code >> 8) & 0xFF
            low = sjis_code & 0xFF
            bytes_data = bytes([high, low])
        
        # 解码为Unicode
        char = bytes_data.decode('shift-jis', errors='ignore')
        return char if char else None
    except:
        return None


def generate_sjis_sequence(total_chars):
    """生成Shift-JIS编码顺序序列"""
    sjis_codes = []
    
    # 单字节ASCII范围 (0x20-0x7E)
    for code in range(0x20, 0x7F):
        sjis_codes.append(code)
        if len(sjis_codes) >= total_chars:
            return sjis_codes
    
    # 半角片假名 (0xA1-0xDF)
    for code in range(0xA1, 0xE0):
        sjis_codes.append(code)
        if len(sjis_codes) >= total_chars:
            return sjis_codes
    
    # 双字节字符范围
    # 第一字节: 0x81-0x9F, 0xE0-0xFC
    # 第二字节: 0x40-0x7E, 0x80-0xFC
    for first in list(range(0x81, 0xA0)) + list(range(0xE0, 0xFD)):
        for second in list(range(0x40, 0x7F)) + list(range(0x80, 0xFD)):
            sjis_code = (first << 8) | second
            sjis_codes.append(sjis_code)
            if len(sjis_codes) >= total_chars:
                return sjis_codes
    
    # 如果还不够，用索引填充
    while len(sjis_codes) < total_chars:
        sjis_codes.append(0)
    
    return sjis_codes


def extract_font_dat(dat_file, output_dir):
    """提取单个 .dat 字库文件"""
    
    # 创建输出目录
    os.makedirs(output_dir, exist_ok=True)
    
    try:
        with open(dat_file, 'rb') as f:
            # 读取32字节头部
            header = f.read(32)
            
            if len(header) < 32:
                print(f"错误：{dat_file} 文件头不完整")
                return False
            
            # 解析头部
            magic = header[0:4]
            try:
                magic_str = magic.decode('ascii', errors='ignore')
            except:
                magic_str = str(magic)
            
            version = struct.unpack('<I', header[4:8])[0]
            bitmap_width = struct.unpack('<H', header[0x10:0x12])[0]
            bitmap_height = struct.unpack('<H', header[0x12:0x14])[0]
            char_width = struct.unpack('<H', header[0x14:0x16])[0]
            char_height = struct.unpack('<H', header[0x16:0x18])[0]
            
            # 输出信息
            print(f"\n{'='*60}")
            print(f"文件: {os.path.basename(dat_file)}")
            print(f"{'='*60}")
            print(f"魔数: {magic_str}")
            print(f"版本: 0x{version:08X}")
            print(f"位图尺寸: {bitmap_width} × {bitmap_height} 像素")
            print(f"字符尺寸: {char_width} × {char_height} 像素")
            
            # 验证参数
            if char_width == 0 or char_height == 0:
                print(f"错误：字符尺寸无效")
                return False
            
            if bitmap_width == 0 or bitmap_height == 0:
                print(f"错误：位图尺寸无效")
                return False
            
            # 计算字符数量
            chars_per_row = bitmap_width // char_width
            chars_per_col = bitmap_height // char_height
            total_chars = chars_per_row * chars_per_col
            
            print(f"每行字符数: {chars_per_row}")
            print(f"总字符数: {total_chars}")
            
            # 读取完整位图数据
            bitmap_size = bitmap_width * bitmap_height
            bitmap_data = f.read(bitmap_size)
            
            if len(bitmap_data) < bitmap_size:
                print(f"警告：位图数据不完整 ({len(bitmap_data)}/{bitmap_size} 字节)")
            
            # 生成Shift-JIS编码序列
            sjis_codes = generate_sjis_sequence(total_chars)
            print(f"已生成 {len(sjis_codes)} 个Shift-JIS编码")
            
            # 创建完整大图
            full_image = Image.new('L', (bitmap_width, bitmap_height), color=0)
            
            # 准备索引文件内容
            index_lines = []
            index_lines.append("索引\tShift-JIS\t字符\t文件名")
            index_lines.append("-" * 60)
            
            # 提取每个字符
            success_count = 0
            char_images = []  # 保存单个字符图像用于生成大图
            
            for idx in range(total_chars):
                row = idx // chars_per_row
                col = idx % chars_per_row
                
                # 创建字符图像 (灰度模式)
                char_img = Image.new('L', (char_width, char_height), color=0)
                
                # 逐像素填充
                for y in range(char_height):
                    for x in range(char_width):
                        src_x = col * char_width + x
                        src_y = row * char_height + y
                        offset = src_y * bitmap_width + src_x
                        
                        if offset < len(bitmap_data):
                            pixel = bitmap_data[offset]
                            char_img.putpixel((x, y), pixel)
                            # 同时填充到大图
                            full_image.putpixel((src_x, src_y), pixel)
                
                # 获取Shift-JIS编码和字符
                sjis_code = sjis_codes[idx] if idx < len(sjis_codes) else 0
                unicode_char = sjis_to_unicode(sjis_code) if sjis_code > 0 else None
                
                # 生成文件名
                if sjis_code > 0:
                    sjis_hex = f"{sjis_code:04X}" if sjis_code > 0xFF else f"{sjis_code:02X}"
                    
                    if unicode_char and unicode_char.isprintable():
                        # 过滤文件系统不允许的字符
                        invalid_chars = '<>:"/\\|?*'
                        safe_char = unicode_char
                        for ic in invalid_chars:
                            safe_char = safe_char.replace(ic, '_')
                        filename = f"{idx:04d}_SJIS_{sjis_hex}_{safe_char}.png"
                        display_char = unicode_char
                    else:
                        filename = f"{idx:04d}_SJIS_{sjis_hex}.png"
                        display_char = ""
                else:
                    filename = f"{idx:04d}.png"
                    sjis_hex = ""
                    display_char = ""
                
                # 保存单个字符图像
                try:
                    output_path = os.path.join(output_dir, filename)
                    char_img.save(output_path)
                    success_count += 1
                except Exception as e:
                    print(f"保存失败 [{idx}]: {e}")
                    filename = f"ERROR_{idx:04d}.png"
                
                # 添加到索引
                index_lines.append(f"{idx:04d}\t{sjis_hex}\t{display_char}\t{filename}")
                
                # 进度显示
                if (idx + 1) % 100 == 0 or idx == total_chars - 1:
                    progress = (idx + 1) / total_chars * 100
                    print(f"进度: {idx + 1}/{total_chars} ({progress:.1f}%)", end='\r')
            
            print(f"\n成功提取 {success_count} 个字符")
            
            # 保存完整大图
            full_image_path = os.path.join(output_dir, "_full_image.png")
            full_image.save(full_image_path)
            print(f"完整字库图: {full_image_path}")
            
            # 保存索引文件
            index_path = os.path.join(output_dir, "_index.txt")
            with open(index_path, 'w', encoding='utf-8') as f:
                f.write('\n'.join(index_lines))
            print(f"索引文件: {index_path}")
            
            # 生成信息文件
            info_path = os.path.join(output_dir, "_info.txt")
            with open(info_path, 'w', encoding='utf-8') as f:
                f.write(f"字库文件: {os.path.basename(dat_file)}\n")
                f.write(f"魔数: {magic_str}\n")
                f.write(f"版本: 0x{version:08X}\n")
                f.write(f"位图尺寸: {bitmap_width} × {bitmap_height}\n")
                f.write(f"字符尺寸: {char_width} × {char_height}\n")
                f.write(f"每行字符: {chars_per_row}\n")
                f.write(f"总字符数: {total_chars}\n")
                f.write(f"编码方式: Shift-JIS\n")
            
            print(f"所有文件已保存到: {output_dir}")
            return True
            
    except FileNotFoundError:
        print(f"错误：文件不存在 - {dat_file}")
        return False
    except Exception as e:
        print(f"错误：处理文件失败 - {e}")
        import traceback
        traceback.print_exc()
        return False


def batch_extract_fonts(directory='.'):
    """批量提取目录下所有 .dat 文件"""
    
    # 查找所有 .dat 文件
    dat_files = list(Path(directory).glob('*.dat'))
    
    if not dat_files:
        print(f"未找到 .dat 文件在目录: {directory}")
        return
    
    print(f"找到 {len(dat_files)} 个 .dat 文件")
    
    success_count = 0
    for dat_file in dat_files:
        # 生成输出目录名（去掉扩展名）
        output_dir = os.path.join(directory, dat_file.stem)
        
        # 提取字库
        if extract_font_dat(str(dat_file), output_dir):
            success_count += 1
    
    print(f"\n{'='*60}")
    print(f"批处理完成: {success_count}/{len(dat_files)} 个文件成功处理")
    print(f"{'='*60}")


if __name__ == "__main__":
    # 提取当前目录所有 .dat 文件
    batch_extract_fonts()
    
    # 或指定目录
    # batch_extract_fonts('D:/fonts')
