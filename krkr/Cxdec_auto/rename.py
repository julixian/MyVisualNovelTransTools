import os
import shutil
import argparse

def parse_manifest(manifest_file):
    """
    解析清单文件，创建哈希名到原始路径的映射。
    """
    hash_to_original = {}
    
    print("正在解析清单文件...")
    try:
        with open(manifest_file, 'r', encoding='utf-8') as f:
            lines = set(f.readlines()) # 去重
            for line in lines:
                line = line.strip()
                if not line:
                    continue
                
                parts = line.split('##YSig##')
                if len(parts) != 2:
                    continue
                
                original_path = parts[0].strip().replace('\\', '/') # 统一路径分隔符
                hashed_name = parts[1].strip()

                if original_path == '%EmptyString%':
                    continue
                
                hash_to_original[hashed_name] = original_path

    except FileNotFoundError:
        print(f"错误：无法打开清单文件 -> {manifest_file}")
        return None
    
    print(f"清单解析完成，共找到 {len(hash_to_original)} 个有效条目。")
    return hash_to_original

def rebuild_full_structure(manifest_file, source_dir, dest_dir):
    """
    根据清单文件，完整地重建文件和目录结构。
    源目录中的文件名和文件夹名都是哈希值。
    """
    # --- 1. 验证输入路径 ---
    if not os.path.isfile(manifest_file):
        return
    if not os.path.isdir(source_dir):
        print(f"错误：源文件夹不存在 -> {source_dir}")
        return

    # --- 2. 解析清单，获取映射关系 ---
    hash_to_original_map = parse_manifest(manifest_file)
    if not hash_to_original_map:
        return

    # --- 3. 准备目标文件夹 ---
    os.makedirs(dest_dir, exist_ok=True)
    print(f"\n结构将重建到: {os.path.abspath(dest_dir)}")
    print("-" * 50)

    # --- 4. 遍历源文件夹，开始重建 ---
    success_files = 0
    success_dirs = 0
    unknown_items = 0

    # 使用一个集合来跟踪已创建的目录，避免重复打印信息
    created_dirs_tracker = set()

    for root, dirs, files in os.walk(source_dir):
        # a. 恢复文件
        for hashed_file in files:
            original_full_path = hash_to_original_map.get(hashed_file)
            
            # 确保找到的是文件条目 (不是以'/'结尾)
            if original_full_path and not original_full_path.endswith('/'):
                # 构造完整的目标文件路径
                target_file_path = os.path.join(dest_dir, original_full_path.replace('/', os.sep))
                source_file_path = os.path.join(root, hashed_file)
                
                try:
                    # 获取目标文件的父目录，并创建它
                    target_parent_dir = os.path.dirname(target_file_path)
                    if target_parent_dir not in created_dirs_tracker:
                        os.makedirs(target_parent_dir, exist_ok=True)
                        created_dirs_tracker.add(target_parent_dir)

                    shutil.copy2(source_file_path, target_file_path)
                    # 打印相对于目标根目录的路径，更清晰
                    print(f"恢复文件: {os.path.relpath(target_file_path, dest_dir)}")
                    success_files += 1
                except Exception as e:
                    print(f"错误: 恢复文件 {os.path.relpath(target_file_path, dest_dir)} 失败 -> {e}")
            else:
                # 如果在清单中找不到，或清单条目是目录，则警告
                print(f"警告: 在清单中未找到文件 {hashed_file} 的有效信息，跳过。")
                unknown_items += 1

        # b. 仅创建清单中明确定义的空目录
        # (因为文件恢复时，其所在目录已被自动创建，这里只需处理那些本身就是清单条目的空目录)
        for hashed_dir in dirs:
            original_dir_path = hash_to_original_map.get(hashed_dir)
            if original_dir_path and original_dir_path.endswith('/'):
                target_dir_path = os.path.join(dest_dir, original_dir_path.replace('/', os.sep))
                if target_dir_path not in created_dirs_tracker:
                    try:
                        os.makedirs(target_dir_path, exist_ok=True)
                        created_dirs_tracker.add(target_dir_path)
                        print(f"创建空目录: {os.path.relpath(target_dir_path, dest_dir)}")
                        success_dirs += 1
                    except Exception as e:
                        print(f"错误: 创建目录 {os.path.relpath(target_dir_path, dest_dir)} 失败 -> {e}")

    # --- 5. 输出总结 ---
    print("-" * 50)
    print("重建完成！")
    print(f"成功恢复文件数: {success_files}")
    print(f"成功创建目录数: {success_dirs}")
    print(f"未知或跳过的项目数: {unknown_items}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="根据清单文件，从全哈希命名的源目录完整重建原始目录结构。")
    parser.add_argument("manifest_file", help="包含文件/目录和哈希值的清单 TXT 文件。")
    parser.add_argument("source_dir", help="存放哈希命名文件和文件夹的源目录。")
    parser.add_argument("dest_dir", help="用于存放完整恢复后项目结构的目标目录。")

    args = parser.parse_args()

    rebuild_full_structure(args.manifest_file, args.source_dir, args.dest_dir)

