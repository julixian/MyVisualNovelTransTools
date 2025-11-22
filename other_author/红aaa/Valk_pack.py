import os

inputdir = "data08"
outputfile = "data08.odn"

dirs = []
offset = len(os.listdir(inputdir)) * 0x10
for name in os.listdir(inputdir):
    size = os.path.getsize(os.path.join(inputdir, name))
    dirs.append({
        "name": name,
        "offset": offset,
        "size": size
    })

    offset += size

with open(outputfile, "wb") as w:
    for entry in dirs:
        fn = entry["name"] + hex(entry["offset"])[2:].zfill(8)
        w.write(fn.encode())

    for entry in dirs:
        with open(os.path.join(inputdir, entry["name"]), "rb") as r:
            w.write(r.read())