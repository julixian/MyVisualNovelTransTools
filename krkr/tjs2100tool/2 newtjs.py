import os
source_dir = "sys" #替换为源文件目录的路径
output_dir = "patch" #替换为输出目录的路径
key = "" #替换为您所需的key值
for filename in os.listdir(source_dir):
    if filename.endswith(".txt"):
        fn = filename.replace(".dump.txt", "")
        if not fn in ["override", "stayontop", "stopdeactive", "snapcapture", "uisystem", "msghack", "", "", "", "", "", "", "", "", "", "", "", "", "", ""]:
            continue
        input_file = os.path.join(source_dir, filename)
        output_file = os.path.join(output_dir, filename)
        os.system("StringTool.exe {} {} {}".format(input_file, key, output_file))
        os.system(f"move {source_dir}\\{filename.replace(".dump.txt", ".tjs.new")} {output_dir}\\{filename.replace('.dump.txt', '.tjs')}")
