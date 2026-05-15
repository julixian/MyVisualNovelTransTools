import os

# 定义输入和输出文件夹
input_folder = "mashiro_fhd_txt_v0409"
output_folder = "output_folder"

# 创建输出文件夹
if not os.path.exists(output_folder):
    os.makedirs(output_folder)

# 遍历输入文件夹中的所有文件
for filename in os.listdir(input_folder):
    if filename.endswith(".txt"):
        # 打开输入文件
        with open(os.path.join(input_folder, filename), "r", encoding="utf-8") as f:
            lines = f.readlines()

        # 创建输出文件
        with open(os.path.join(output_folder, filename), "w", encoding="utf-8") as f:
            for i in range(0, len(lines), 2):
                # 获取 ☆ 行和 ★ 行
                star_line = lines[i].strip()
                double_star_line = lines[i + 1].strip()

                # 检查行是否为空
                if not star_line or not double_star_line:
                    continue

                # 提取 ☆ 行和 ★ 行的数字
                star_id = int(star_line[1:7])
                double_star_id = int(double_star_line[1:7])

                # 替换 ☆ 行的数字
                new_star_line = f"☆{star_id:06d}{star_line[7:]}"
                new_double_star_line = f"★{double_star_id:06d}{double_star_line[7:]}"

                # 写入输出文件
                f.write(new_star_line)
                f.write(new_double_star_line)
