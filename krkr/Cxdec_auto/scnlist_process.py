import os
FileNameHashLogPath = "StringHashDumper_Output\\FileNameHash.log"
Extractor_DataPath = "Extractor_Output\\data"
scnlistFileHash = None
with open(FileNameHashLogPath, "r", encoding="utf-16-le") as f:
    hashlist = f.readlines()
    for l in hashlist:
        l = l.strip("\n")
        if l .startswith("!scnlist.txt"):
            l = l.split("##")
            scnlistFileHash = l[2]
            break
    if scnlistFileHash is None:
        print("Error: !scnlist.txt hash not found.")
        exit()
scnlistPath = None
for root, dirs, files in os.walk(Extractor_DataPath):
    for file in files:
        file_path = os.path.join(root, file)
        file_hash = os.path.splitext(file)[0]
        if file_hash == scnlistFileHash:
            scnlistPath = file_path
if scnlistPath is None:
    print("Error: !scnlist.txt not found.")
    exit()

try:
    scnlist = open(scnlistPath, "r", encoding="sjis").readlines()
except:
    scnlist = open(scnlistPath, "r", encoding="utf-16").readlines()
res = []
for l in scnlist:
    l = l.strip().strip("#").strip("\n").strip("\t").lower()
    if l.endswith(".ks") or l.endswith(".txt"):
        res.append(l + ".scn")
res = "\n".join(res)
with open("files.txt", "w", encoding="utf-16") as f:
    f.write(res)