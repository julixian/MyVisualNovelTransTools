# -*- coding=utf-8 -*-
import json
import sys
import os
import shutil
import re

lang_num = int(sys.argv[1])
append = int(sys.argv[2])
fusers = [
    "%p-1;%fＭＳ ゴシック;――%p;%fuser;",
    "%p-1;%fＭＳ ゴシック;——%p;%fuser;",
    "%fＭＳ ゴシック;――%p;%fuser;",
    "%p-1;%fＭＳ ゴシック;%p;%fuser;",
    "%",
    "\n", 
]

def fuser_helper(text: str):
    for f in fusers:
        text = text.replace(f, "——")
    return text

def replace_user(j, new_text, jp_text):
    if r'%p' in jp_text:
        mtext = fuser_helper(new_text)
    elif r"\n" in new_text:
        mtext = new_text
    elif r'\n' in jp_text:
        mtext = new_text
    elif r'[' in jp_text:
        mtext = new_text
    else:
        return new_text
    ltext = mtext.replace(r"\n", "")
    if len(j) > 4:
        j[3], j[4] = ltext, ltext
    else:
        j.append(ltext)
        j.append(ltext)
    return mtext

def normalize_text(text):
    if text is None:
        return None
    return text

def find_translation_by_jp_text(jp_text, texts):
    """通过日文原文查找对应的中文翻译"""
    for text in texts:
        if normalize_text(text['jp_text']) == normalize_text(jp_text):
            return text['cn_text']
        # 处理全角空格差异
        if '　' in text['jp_text'] or '　' in jp_text:
            text_no_space = text['jp_text'].replace('　', '')
            jp_no_space = jp_text.replace('　', '')
            if text_no_space == jp_no_space:
                return text['cn_text']
    return None

def process_phonechat_by_lookup(data, texts):
    """处理各种数据结构中的phonechat，通过原文查找翻译"""
    if isinstance(data, dict):
        for key, value in data.items():
            if key == "phonechat":
                process_phonechat_content(value, texts)
            else:
                process_phonechat_by_lookup(value, texts)
    elif isinstance(data, list):
        for i, item in enumerate(data):
            if (isinstance(item, list) and len(item) > 0 and 
                isinstance(item[0], str) and item[0] == "phonechat"):
                process_phonechat_array_format(item, texts)
            else:
                process_phonechat_by_lookup(item, texts)

def process_phonechat_array_format(phonechat_array, texts):
    """处理数组格式的phonechat命令"""
    i = 0
    while i < len(phonechat_array):
        if (phonechat_array[i] == "text" and 
            i + 1 < len(phonechat_array) and 
            phonechat_array[i + 1] is not None and
            isinstance(phonechat_array[i + 1], str)):
            
            jp_text = phonechat_array[i + 1]
            cn_text = find_translation_by_jp_text(jp_text, texts)
            if cn_text:
                phonechat_array[i + 1] = cn_text
        i += 1

def process_phonechat_content(phonechat_data, texts):
    """处理phonechat的具体内容"""
    if not phonechat_data:
        return
    
    # 格式1：对象数组格式
    if isinstance(phonechat_data, list) and len(phonechat_data) > 0 and isinstance(phonechat_data[0], dict):
        for chat_item in phonechat_data:
            if isinstance(chat_item, dict) and 'text' in chat_item and chat_item['text']:
                jp_text = chat_item['text']
                cn_text = find_translation_by_jp_text(jp_text, texts)
                if cn_text:
                    chat_item['text'] = cn_text
    
    # 格式2：平铺数组格式
    elif isinstance(phonechat_data, list):
        i = 0
        while i < len(phonechat_data):
            if (phonechat_data[i] == "text" and 
                i + 1 < len(phonechat_data) and 
                phonechat_data[i + 1] is not None and
                isinstance(phonechat_data[i + 1], str)):
                
                jp_text = phonechat_data[i + 1]
                cn_text = find_translation_by_jp_text(jp_text, texts)
                if cn_text:
                    phonechat_data[i + 1] = cn_text
            i += 1

def process_scenes_arrays(scenes, texts):
    """处理scenes中的各种数组格式，包括phonechat命令"""
    for scene in scenes:
        if isinstance(scene, list):
            for item in scene:
                if (isinstance(item, list) and len(item) > 0 and 
                    isinstance(item[0], str) and item[0] == "phonechat"):
                    process_phonechat_array_format(item, texts)

error_log = []
success_count = 0
total_count = 0

for fn in os.listdir('scn'):
    if fn.endswith('.json') and not fn.endswith('.resx.json'):
        total_count += 1
        try:
            # 统一使用UTF-8编码读取JSON文件
            with open(f'scn/{fn}', 'r', encoding='utf-8') as f:
                script_data = json.load(f)
            
            # 统一使用UTF-8编码读取文本文件
            with open(f'new_txt/{fn}.txt', 'r', encoding='utf-8') as texts_file:
                file_content = texts_file.read()
            
            texts = []
            lines = file_content.split('\n')
            i = 0
            while i < len(lines):
                magic = lines[i]
                if not magic or magic[0] not in ['★', '☆', '@', '▶', '▷']:
                    i += 1
                    continue
                if not any(char in magic[1:10] for char in ['★', '☆', '@', '◀', '◁']):
                    raise ValueError(f'[Error {fn}] Invalid magic: {magic}')
                jptext = magic.strip()[9:]
                if jptext and jptext[0] in ['★', '☆', '@', '◀', '◁']:
                    jptext = jptext[1:]
                jptext = jptext.replace('\\n', '\n').replace('\\\n', '\\n')
                
                i += 1
                if i >= len(lines):
                    raise ValueError(f'[Error {fn}] Missing cn_text after: {magic}')
                
                cn_text = lines[i]
                # 跳过注释行
                while cn_text.startswith('/'):
                    i += 1
                    if i >= len(lines):
                        raise ValueError(f'[Error {fn}] Missing cn_text after comment and: {magic}')
                    cn_text = lines[i]
                
                str_id = int(magic[0:7].replace('★', '').replace('☆', '').replace('@', '').replace('▶', '').replace('◀', '').replace('▷', '').replace('◁', ''))
                if cn_text and cn_text[0] in ['★', '☆', '@', '▶', '▷']:
                    if not any(char in cn_text[1:10] for char in ['★', '☆', '@', '◀', '◁']):
                        raise ValueError(f'[Error {fn}] Invalid cn_text magic: {cn_text}')
                    cn_text = cn_text[9:]
                if cn_text and cn_text[0] in ['★', '☆', '@', '▶', '▷']:
                    cn_text = cn_text[1:]
                cn_text = cn_text.replace('\\n', '\n').replace('\\\n', '\\n')
                str_rep = {'id': str_id, 'cn_text': cn_text, 'jp_text': jptext}
                texts.append(str_rep)
                i += 1

            # 处理phonechat（静默处理，不输出信息）
            process_phonechat_by_lookup(script_data, texts)
            
            # 处理scenes中的数组格式phonechat命令
            if 'scenes' in script_data:
                process_scenes_arrays(script_data['scenes'], texts)

            def get_text(text_id):
                for text in texts:
                    if text['id'] == text_id:
                        return text['cn_text'], text['jp_text']
                return None

            scenes = script_data['scenes']
            index = 0
            for i in scenes:
                if 'texts' in i:
                    j_texts = i['texts']
                    for j in j_texts:
                        if j[0] is not None and j[0] != '':
                            new_text, jp_text = get_text(index)
                            if not new_text:
                                continue
                            
                            if normalize_text(j[0]) != normalize_text(jp_text):
                                # 对于全角空格的特殊处理
                                if '　' in j[0] or '　' in jp_text:
                                    j0_no_space = j[0].replace('　', '')
                                    jp_no_space = jp_text.replace('　', '')
                                    if j0_no_space != jp_no_space:
                                        raise ValueError(f'[Error {fn}] Text mismatch: "{j[0]}" != "{jp_text}"')
                                else:
                                    raise ValueError(f'[Error {fn}] Text mismatch: "{j[0]}" != "{jp_text}"')
                            
                            j[0] = replace_user(j, new_text, jp_text)
                            index += 1
                        size = len(j[1])
                        if append:
                            j[1].append([None, None, None])
                            if j[1][lang_num][0] is not None and j[1][lang_num][0] != '':
                                new_text, jp_text = get_text(index)
                                if not new_text:
                                    continue
                                
                                if normalize_text(j[1][lang_num][0]) != normalize_text(jp_text):
                                    if '　' in j[1][lang_num][0] or '　' in jp_text:
                                        j1_no_space = j[1][lang_num][0].replace('　', '')
                                        jp_no_space = jp_text.replace('　', '')
                                        if j1_no_space != jp_no_space:
                                            raise ValueError(f'[Error {fn}] Text mismatch (append): "{j[1][lang_num][0]}" != "{jp_text}"')
                                    else:
                                        raise ValueError(f'[Error {fn}] Text mismatch (append): "{j[1][lang_num][0]}" != "{jp_text}"')
                                
                                j[1][size][0] = replace_user(j[1][size], new_text, jp_text)
                                index += 1
                            if j[1][lang_num][1] is not None and j[1][lang_num][1] != '':
                                new_text, jp_text = get_text(index)
                                if not new_text:
                                    continue
                                
                                if normalize_text(j[1][lang_num][1]) != normalize_text(jp_text):
                                    if '　' in j[1][lang_num][1] or '　' in jp_text:
                                        j1_no_space = j[1][lang_num][1].replace('　', '')
                                        jp_no_space = jp_text.replace('　', '')
                                        if j1_no_space != jp_no_space:
                                            raise ValueError(f'[Error {fn}] Text mismatch (append): "{j[1][lang_num][1]}" != "{jp_text}"')
                                    else:
                                        raise ValueError(f'[Error {fn}] Text mismatch (append): "{j[1][lang_num][1]}" != "{jp_text}"')
                                
                                lt = replace_user(j[1][size], new_text, jp_text)
                                j[1][size][1] = lt
                                j[1][size][2] = len(lt)
                                index += 1
                        else: # not append
                            if j[1][lang_num][0] is not None and j[1][lang_num][0] != '':
                                new_text, jp_text = get_text(index)
                                if not new_text:
                                    continue
                                
                                if normalize_text(j[1][lang_num][0]) != normalize_text(jp_text):
                                    if '　' in j[1][lang_num][0] or '　' in jp_text:
                                        j1_no_space = j[1][lang_num][0].replace('　', '')
                                        jp_no_space = jp_text.replace('　', '')
                                        if j1_no_space != jp_no_space:
                                            raise ValueError(f'[Error {fn}] Text mismatch (not append): "{j[1][lang_num][0]}" != "{jp_text}"')
                                    else:
                                        raise ValueError(f'[Error {fn}] Text mismatch (not append): "{j[1][lang_num][0]}" != "{jp_text}"')
                                
                                j[1][lang_num][0] = replace_user(j[1][lang_num], new_text, jp_text)
                                index += 1
                            if j[1][lang_num][1] is not None and j[1][lang_num][1] != '':
                                new_text, jp_text = get_text(index)
                                if not new_text:
                                    continue
                                
                                if normalize_text(j[1][lang_num][1]) != normalize_text(jp_text):
                                    if '　' in j[1][lang_num][1] or '　' in jp_text:
                                        j1_no_space = j[1][lang_num][1].replace('　', '')
                                        jp_no_space = jp_text.replace('　', '')
                                        if j1_no_space != jp_no_space:
                                            raise ValueError(f'[Error {fn}] Text mismatch (not append): "{j[1][lang_num][1]}" != "{jp_text}"')
                                    else:
                                        raise ValueError(f'[Error {fn}] Text mismatch (not append): "{j[1][lang_num][1]}" != "{jp_text}"')

                                lt = replace_user(j[1][lang_num], new_text, jp_text)
                                j[1][lang_num][1] = lt
                                j[1][lang_num][2] = len(lt)
                                if not re.match(r"\A[%A-Za-z&_\d;]*\z", lt):
                                    pure_backlog = lt
                                    pure_backlog = re.sub(r"%f.*;(.*?)%r", r"\1", pure_backlog)
                                    pure_backlog = re.sub(r"%.+?;", "", pure_backlog)
                                    if len(j[1][lang_num]) >= 4:
                                        if j[1][lang_num][3] is not None and j[1][lang_num][3] != '':
                                            j[1][lang_num][3] = pure_backlog
                                    if len(j[1][lang_num]) >= 5:
                                        if j[1][lang_num][4] is not None and j[1][lang_num][4] != '':
                                            j[1][lang_num][4] = pure_backlog
                                index += 1

                if 'selects' in i:
                    selects = i['selects']
                    for s in selects:
                        if 'language' in s and lang_num > 0 and s['language'][lang_num]['text'] is not None and s['language'][lang_num]['text'] != '':
                            new_text, jp_text = get_text(index)
                            if not new_text:
                                continue
                            
                            if normalize_text(s['language'][lang_num]['text']) != normalize_text(jp_text):
                                if '　' in s['language'][lang_num]['text'] or '　' in jp_text:
                                    s_text_no_space = s['language'][lang_num]['text'].replace('　', '')
                                    jp_no_space = jp_text.replace('　', '')
                                    if s_text_no_space != jp_no_space:
                                        raise ValueError(f'[Error {fn}] Text mismatch (selects/language): "{s["language"][lang_num]["text"]}" != "{jp_text}"')
                                else:
                                    raise ValueError(f'[Error {fn}] Text mismatch (selects/language): "{s["language"][lang_num]["text"]}" != "{jp_text}"')

                            if append:
                                s['language'].append({'text': new_text})
                            else:
                                s['language'][lang_num]['text'] = new_text
                            index += 1
                        elif 'text' in s and s['text'] is not None and s['text'] != '':
                            new_text, jp_text = get_text(index)
                            if not new_text:
                                continue
                            
                            if normalize_text(s['text']) != normalize_text(jp_text):
                                if '　' in s['text'] or '　' in jp_text:
                                    s_text_no_space = s['text'].replace('　', '')
                                    jp_no_space = jp_text.replace('　', '')
                                    if s_text_no_space != jp_no_space:
                                        raise ValueError(f'[Error {fn}] Text mismatch (selects/text): "{s["text"]}" != "{jp_text}"')
                                else:
                                    raise ValueError(f'[Error {fn}] Text mismatch (selects/text): "{s["text"]}" != "{jp_text}"')
                            
                            if append and 'language' in s:
                                s['language'].append({'text': new_text})
                            elif append and 'language' not in s:
                                s['language'] = [None, {'text': new_text}]
                            else:
                                s['text'] = new_text
                            index += 1

            # 统一使用UTF-8编码写入JSON文件
            with open(f'scn/{fn}', 'w', encoding='utf-8') as complete_file:
                json.dump(script_data, complete_file, ensure_ascii=False, indent=4, sort_keys=False)
            
            success_count += 1

        except Exception as e:
            error_log.append(f"Error processing file {fn}: {e}")
            if not os.path.exists("error"):
                os.makedirs("error")
            try:
                shutil.copy(f"new_txt/{fn}.txt", f"error/{fn}.txt")
            except:
                pass

# 只在有错误时才输出信息
if error_log:
    with open("error_log.txt", "w", encoding="utf-8") as f:
        for error in error_log:
            f.write(error + "\n")
    print(f"处理完成：成功 {success_count}/{total_count} 个文件")
    print(f"遇到 {len(error_log)} 个错误，详情请查看 error_log.txt")
else:
    print(f"处理完成：成功处理所有 {success_count} 个文件")
