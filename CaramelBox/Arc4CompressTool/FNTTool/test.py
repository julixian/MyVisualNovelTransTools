from PIL import Image, ImageDraw, ImageFont
import os, json, re, io

def open_file_b(path):
    with open(path, 'rb') as f:
        return f.read()
    
def open_json(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)
    
def to_bytes(lst:int):
    return lst.to_bytes(1, byteorder='little')
    

#字体
font = ImageFont.truetype(r"F:\game\LOOPERS\GameData\dat\SourceHanSansCN-Medium.ttf", 0x18 - 4)
with open('sjis_list.txt', 'r', encoding='utf-8') as f:
    sjis_list = f.read()

data = open('fntnormal.fnt', 'rb').read()
data = io.BytesIO(data)
_ = open_json('subs_cn_jp_v1.json')
replace_dict = {}
for k in _:
    replace_dict[_[k]] = k
def get_text_img(data, text):
    text = text.split()
    res = []
    for char in text:
        try:
            b = char.encode("932")
        except:
            raise Exception("Cannot encode character with cp932: " + char)
        str1 = b[0]
        if len(b) == 1:
            res.append(get_img_from_font(data, str1, mode=1))
        else:
            idx = sjis_list.index(char)
            res.append(get_img_from_font(data, idx, mode=2))
    return res

def chang_fontimg(data, index, replace_dict):
    data.seek(0x20014 + 0x18 * 0xc * 0x4 * 157  + 0x18 * 0x18 * index * 0x4)
    w = 0x18
    h = 0x18
    char = sjis_list[index]
    draw_char = replace_dict.get(char, char)
    new_img = Image.new('RGBA', (w, h))
    draw = ImageDraw.Draw(new_img)
    #绘制参数
    draw.text((2, -4), draw_char, font=font, fill=(255, 255, 255, 255), stroke_width= 2, stroke_fill=(0x2d, 0x14, 0x82, 255))
    new_img = new_img.transpose(Image.FLIP_TOP_BOTTOM)
    char_data = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = new_img.getpixel((x, y))
            char_data.append(b)
            char_data.append(g)
            char_data.append(r)
            char_data.append(a)
    for i in range(w * h * 4):
        data.write(to_bytes(char_data[i]))
    

def get_img_from_font(data, index, mode = 2):
    if mode == 2:
        data.seek(0x20014 + 0x18 * 0xc * 0x4 * 157  + 0x18 * 0x18 * index * 0x4)
        w = 0x18
        h = 0x18
    else:
        if index > 157:
            raise Exception("Index out of range")
        data.seek(0x20014 + 0x18 * 0xc * 0x4 * index)
        w = 0x18
        h = 0x0c
    img = Image.new('RGBA', (w, h))
    i = 0
    while i < w * h:
        b = data.read(1)
        g = data.read(1)
        r = data.read(1)
        a = data.read(1)
        img.putpixel((i % w, i // w), (ord(r), ord(g), ord(b), ord(a)))
        i += 1
    img = img.transpose(Image.FLIP_TOP_BOTTOM)
    return img

for i in range(len(sjis_list)):
    chang_fontimg(data, i, replace_dict)

get_text_img(data, "＂")[0].save('test.png')

with open('fntnormal_new.fnt', 'wb') as f:
    f.write(data.getvalue())