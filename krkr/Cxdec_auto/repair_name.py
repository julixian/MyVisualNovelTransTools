import os
import shutil

ori_extpath = "Extractor_Output"
outpath = "repaired"

hashpair = open("files_match.txt", "r", encoding="utf-16-le").readlines()
hashdic = {}
for l in hashpair:
    l = l.strip("\n")
    filename, hash = l.split(",")
    hashdic[hash] = filename
    
if os.path.exists(outpath):
    os.chmod(outpath, 0o777)
    for root, dirs, files in os.walk(outpath):
        for dir in dirs:
            os.chmod(os.path.join(root, dir), 0o777)
        for file in files:
            os.chmod(os.path.join(root, file), 0o777)
    shutil.rmtree(outpath)
os.makedirs(outpath)

for root, dirs, files in os.walk(ori_extpath):
    for file in files:
        file_path = os.path.join(root, file)
        file_hash = os.path.splitext(file)[0]
        if file_hash in hashdic:
            new_name = hashdic[file_hash]
            new_path = os.path.join(outpath, new_name)
            shutil.copy2(file_path, new_path)
