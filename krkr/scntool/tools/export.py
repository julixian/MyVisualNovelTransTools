# -*- coding=utf-8 -*-
import json
import codecs
import sys
import os
lang_num = int(sys.argv[1])
for fn in os.listdir('scn'):
    if fn.endswith('.json') and not fn.endswith('.resx.json'):
        with open('scn/%s'%fn, "r", encoding="utf-8") as f:
            script_data = json.load(f)
        objects_keys = script_data.keys()
        with open('txt/%s.txt'%fn, "w", encoding="utf-8") as texts_file:
            scenes = script_data['scenes']
            index = 0
            texts_file.writelines('\n')
            for i in scenes:
                if 'texts' in i:
                    texts = i['texts']
                    for j in texts:
                        if j[0] is not None and j[0] != '':
                            texts_file.writelines('☆%06dN☆'%(index)+j[0].replace('\\n', '\\\n').replace('\n', '\\n') + '\n')
                            texts_file.writelines('★%06dN★'%(index)+j[0].replace('\\n', '\\\n').replace('\n', '\\n') + '\n\n')
                            index += 1
                        if j[1][lang_num][0] is not None and j[1][lang_num][0] != '':
                            texts_file.writelines('☆%06dN2☆'%(index)+j[1][lang_num][0].replace('\\n', '\\\n').replace('\n', '\\n') + '\n')
                            texts_file.writelines('★%06dN2★'%(index)+j[1][lang_num][0].replace('\\n', '\\\n').replace('\n', '\\n') + '\n\n')
                            index += 1
                        if j[1][lang_num][1] is not None and j[1][lang_num][1] != '':
                            texts_file.writelines('☆%06dT☆'%(index)+j[1][lang_num][1].replace('\\n', '\\\n').replace('\n', '\\n') + '\n')
                            texts_file.writelines('★%06dT★'%(index)+j[1][lang_num][1].replace('\\n', '\\\n').replace('\n', '\\n') + '\n\n')
                            index += 1
                if 'selects' in i:
                    selects = i['selects']
                    for s in selects:
                        if 'language' in s and lang_num > 0:
                            ss = s['language']
                            if ss[lang_num]['text'] is not None and ss[lang_num]['text'] != '':
                                texts_file.writelines('☆%06dS☆'%(index)+ss[lang_num]['text'].replace('\\n', '\\\n').replace('\n', '\\n') + '\n')
                                texts_file.writelines('★%06dS★'%(index)+ss[lang_num]['text'].replace('\\n', '\\\n').replace('\n', '\\n') + '\n\n')
                                index += 1
                        elif 'text' in s:
                            text = s['text']
                            if text is not None and text != '':
                                texts_file.writelines('☆%06dS☆'%(index)+text.replace('\\n', '\\\n').replace('\n', '\\n') + '\n')
                                texts_file.writelines('★%06dS★'%(index)+text.replace('\\n', '\\\n').replace('\n', '\\n') + '\n\n')
                                index += 1
            texts_file.flush()
            texts_file.close()