import re
import os

def replace_tags_sequentially(source_file, target_file, output_file):
    """
    读取 source_file 中的标签，按顺序替换 target_file 中的标签，
    并将结果保存到 output_file。
    """
    
    # 1. 定义标签的正则表达式
    # 解释: 
    # [☆★]  : 以实心或空心星号开头
    # \d{6}  : 匹配6位数字
    # [A-Z]  : 匹配一个大写字母 (如 T, N)
    # [☆★]  : 以实心或空心星号结尾
    tag_pattern = re.compile(r'([☆★]\d{6}[A-Z]\d?[☆★])')

    try:
        # 2. 读取第一个文件 (Source) 并提取所有标签
        with open(source_file, 'r', encoding='utf-8') as f_src:
            source_content = f_src.read()
            # findall 会返回一个列表，包含所有按顺序找到的标签
            source_tags = tag_pattern.findall(source_content)

        print(f"从 {source_file} 中提取到了 {len(source_tags)} 个标签。")

        # 3. 读取第二个文件 (Target)
        with open(target_file, 'r', encoding='utf-8') as f_tgt:
            target_content = f_tgt.read()
            # 计算一下目标文件有多少个标签，用于对比
            target_tag_count = len(tag_pattern.findall(target_content))
            
        print(f"在 {target_file} 中发现了 {target_tag_count} 个标签。")

        # 4. 检查标签数量是否一致 (仅作提示，不阻止运行)
        if len(source_tags) != target_tag_count:
            print("⚠️ 注意：两个文件的标签数量不一致！")
            print("   - 如果源文件标签较少，目标文件末尾的标签将不会被替换。")
            print("   - 如果源文件标签较多，多余的标签将被忽略。")
        
        # 5. 执行替换逻辑
        # 创建一个迭代器，用于按顺序取出源标签
        tags_iter = iter(source_tags)

        def replace_callback(match):
            """
            这是 re.sub 的回调函数。
            每当在目标文件中找到一个标签时，就从源标签列表中取下一个来替换它。
            """
            try:
                return next(tags_iter)
            except StopIteration:
                # 如果源标签用完了，就保持原样
                return match.group(0)

        # 使用 re.sub 进行替换
        new_content = tag_pattern.sub(replace_callback, target_content)

        # 6. 保存结果
        with open(output_file, 'w', encoding='utf-8') as f_out:
            f_out.write(new_content)

        print(f"✅ 处理完成！结果已保存至: {output_file}")

    except FileNotFoundError as e:
        print(f"❌ 错误：找不到文件 - {e}")
    except Exception as e:
        print(f"❌ 发生未知错误：{e}")

# ==========================================
# 配置区域：请在这里修改你的文件名
# ==========================================

# 第一个TXT（提供正确标签的文件）
file_1_path = 'txt/バンド411_11月（初デート）.ks.json.txt' 

# 第二个TXT（需要修改标签的文件）
file_2_path = 'new_txt__/バンド411_11月（初デート）.ks.json.txt'

# 输出的新文件
output_path = 'バンド411_11月（初デート）.ks.json.txt'

# 运行函数
if __name__ == '__main__':
    replace_tags_sequentially(file_1_path, file_2_path, output_path)
