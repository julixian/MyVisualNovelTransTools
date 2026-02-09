import os
import shutil

def copy_matching_files(ref_dir, source_dir, dest_dir):
    """
    ref_dir: 参考文件夹 (包含 .json.txt 的文件夹)
    source_dir: 源文件夹 (包含 .scn 文件的查找位置)
    dest_dir: 目标文件夹 (文件最终复制到的位置)
    """

    # 1. 检查目标文件夹是否存在，不存在则创建
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
        print(f"已创建目标文件夹: {dest_dir}")

    # 2. 遍历参考文件夹
    print(f"正在遍历: {ref_dir} ...")
    count = 0
    
    for filename in os.listdir(ref_dir):
        # 3. 筛选以 .json.txt 结尾的文件
        if filename.endswith('.json.txt'):
            # 4. 构造新的文件名 (.json.txt -> .scn)
            # 使用切片去掉最后9个字符 (.json.txt)，然后加上 .scn
            scn_filename = filename[:-9] + ".scn"
            
            # 构造完整的源文件路径 (在第二个文件夹中查找)
            source_file_path = os.path.join(source_dir, scn_filename)
            
            # 构造完整的目标文件路径
            dest_file_path = os.path.join(dest_dir, scn_filename)

            # 5. 检查 .scn 文件是否在第二个文件夹中存在
            if os.path.exists(source_file_path):
                try:
                    # 6. 复制文件
                    shutil.copy2(source_file_path, dest_file_path)
                    print(f"[成功] 已复制: {scn_filename}")
                    count += 1
                except Exception as e:
                    print(f"[错误] 复制 {scn_filename} 失败: {e}")
            else:
                print(f"[跳过] 未找到文件: {scn_filename} (在源目录中不存在)")

    print(f"\n处理完成。共复制了 {count} 个文件到 {dest_dir}。")

# ================= 配置区域 =================
# 请在这里将路径替换为你实际的文件夹路径
# 注意：Windows路径如果包含反斜杠 \，建议在引号前加 r，例如 r"C:\Users\test"

# 1. 参考文件夹 (里面有 .json.txt 文件)
folder_reference = r"new_txt__" 

# 2. 查找文件夹 (里面有 .scn 文件)
folder_search = r"new_scn"

# 3. 目标文件夹 (文件要复制到这里)
folder_output = r"peeked_scn"

# ===========================================

if __name__ == '__main__':
    # 检查输入路径是否存在
    if os.path.exists(folder_reference) and os.path.exists(folder_search):
        copy_matching_files(folder_reference, folder_search, folder_output)
    else:
        print("错误：请检查'参考文件夹'或'查找文件夹'的路径是否正确。")
