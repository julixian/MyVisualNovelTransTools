import os
import argparse
import itertools

def generate_storage_paths(target_directory):
    """
    遍历指定目录，为其中的每个子目录以及子目录的两两组合生成代码并打印。

    :param target_directory: 要扫描的目标文件夹路径。
    """
    # 1. 验证输入路径是否存在且是一个目录
    if not os.path.isdir(target_directory):
        print(f"错误：提供的路径 '{target_directory}' 不是一个有效的文件夹或不存在。")
        return

    print(f"--- 正在扫描文件夹: {os.path.abspath(target_directory)} ---")
    
    found_dirs = []
    
    # 2. 遍历目标目录中的所有项目
    try:
        for item_name in os.listdir(target_directory):
            full_path = os.path.join(target_directory, item_name)
            if os.path.isdir(full_path):
                found_dirs.append(item_name)
    except OSError as e:
        print(f"错误：无法访问文件夹 '{target_directory}'。请检查权限。")
        print(f"详细信息: {e}")
        return

    # 3. 对找到的文件夹进行排序
    found_dirs.sort()

    if not found_dirs:
        print("\n在该文件夹下没有找到任何子文件夹。")
        return
        
    # --- 第一部分：输出单层目录路径 ---
    print(f"\n--- [ 单层目录路径 (共 {len(found_dirs)} 个) ] ---")
    for dir_name in found_dirs:
        print(f'Storages.addAutoPath(System.exePath + "data.xp3>{dir_name}/");')

    # --- 第二部分：输出两两组合的目录路径 ---
    # 使用 itertools.permutations 生成所有长度为2的排列组合
    # 例如：['A', 'B'] -> [('A', 'B'), ('B', 'A')]
    combinations = list(itertools.permutations(found_dirs, 2))
    
    if combinations:
        print(f"\n--- [ 两两组合路径 (共 {len(combinations)} 个) ] ---")
        for dir1, dir2 in combinations:
            # 拼接路径并按指定格式打印
            combined_path = f"{dir1}/{dir2}"
            print(f'Storages.addAutoPath(System.exePath + "data.xp3>{combined_path}/");')

    print(f"\n--- 完成！共找到 {len(found_dirs)} 个子文件夹，生成了 {len(combinations)} 种组合。 ---")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="扫描文件夹，为其子目录及子目录的两两组合生成代码行。",
        epilog="示例: python generate_path_combinations.py C:\\path\\to\\data"
    )
    parser.add_argument(
        "target_directory", 
        help="需要扫描其子文件夹的目标文件夹路径。"
    )

    args = parser.parse_args()
    
    generate_storage_paths(args.target_directory)
