import os
import shutil

def search_and_process_files(original_directory, output_directory, search_keywords):
    # 创建输出目录
    os.makedirs(output_directory, exist_ok=True)

    # 遍历原始文件目录下的所有文件
    for filename in os.listdir(original_directory):
        # 构建原始文件路径
        original_file_path = os.path.join(original_directory, filename)

        # 检查原始文件是否存在
        if not os.path.isfile(original_file_path):
            print(f"File '{original_file_path}' not found!")
            continue

        # 构建输出文件路径
        output_file_path = os.path.join(output_directory, filename)

        # 判断文件是否符合搜索条件的标志位
        matched = False

        try:
            with open(original_file_path, 'r', encoding='utf-8') as original_file:
                for line in original_file:
                    # 只处理★行的内容，忽略☆行
                    if line.startswith('★'):
                        # 检查是否包含搜索关键词
                        if any(keyword in line for keyword in search_keywords):
                            # 将搜索到的行写入输出文件
                            matched = True
                            break

            # 如果文件符合搜索条件，则拷贝到输出路径
            if matched:
                shutil.copyfile(original_file_path, output_file_path)
        except Exception as e:
            print(f"Error processing file '{original_file_path}': {str(e)}")

if __name__ == '__main__':
    original_directory = 'output'  # 原始文件目录路径
    output_directory = 'output_directory'  # 输出文件目录路径

    search_keywords = [
        "お前が入部するまでは平和だったんだ",
#        "%p-1;%fＭＳ ゴシック;——%p;%fuser;",
#        "%fＭＳ ゴシック;――%p;%fuser;",
#        "%p-1;%fＭＳ ゴシック;%p;%fuser;",
    ]

    search_and_process_files(original_directory, output_directory, search_keywords)