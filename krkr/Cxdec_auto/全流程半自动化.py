import os
import shutil


LE_Path = input("请输入LE文件夹中LEProc.exe的路径（不能包含空格）（如果游戏需要转区运行，请填入）：")
if not LE_Path.lower().endswith("leproc.exe"):
    isLE = False
    print("未找到LEProc.exe，将不会转区启动游戏。")
else:
    isLE = True
LE_Path = os.path.normpath(LE_Path)

GAME_Path = input("请输入游戏主程序exe的路径：")
if not GAME_Path.lower().endswith(".exe"):
    print("输入的路径不是exe文件，程序将退出。")
    exit()
GAME_Path = os.path.normpath(GAME_Path)
GAME_BasePath = os.path.dirname(GAME_Path)

print("在接下来的对话框中选择 加载解包模块 ，然后将游戏的data.xp3，以及名字类似scn.xp3\scenario.xp3的封包拖入生成的窗口内，等待解包完成。解包完成后请关闭游戏。")
input("按回车键将开始第一步处理...")

if os.path.exists(os.path.join(GAME_BasePath, "version.dll")):
    os.remove(os.path.join(GAME_BasePath, "version.dll"))

if isLE:
    os.system(f'{LE_Path} CxdecExtractorLoader.exe "{GAME_Path}"')
else:
    os.system(f'CxdecExtractorLoader.exe "{GAME_Path}"')

input("按回车键继续。")
if os.path.exists("Extractor_Output"):
    os.chmod("Extractor_Output", 0o777)
    for root, dirs, files in os.walk("Extractor_Output"):
        for dir in dirs:
            os.chmod(os.path.join(root, dir), 0o777)
        for file in files:
            os.chmod(os.path.join(root, file), 0o777)
    shutil.rmtree("Extractor_Output")
shutil.move(os.path.join(GAME_BasePath, "Extractor_Output"), ".")

print("在接下来的对话框中选择 加载字符串hash提取模块，然后在游戏中过到第一句对话，然后关闭游戏。")
input("按回车键将开始第二步处理...")
if isLE:
    os.system(f'{LE_Path} CxdecExtractorLoader.exe "{GAME_Path}"')
else:
    os.system(f'CxdecExtractorLoader.exe "{GAME_Path}"')
input("按回车键继续。")

if os.path.exists("StringHashDumper_Output"):
    os.chmod("StringHashDumper_Output", 0o777)
    for root, dirs, files in os.walk("StringHashDumper_Output"):
        for dir in dirs:
            os.chmod(os.path.join(root, dir), 0o777)
        for file in files:
            os.chmod(os.path.join(root, file), 0o777)
    shutil.rmtree("StringHashDumper_Output")
shutil.move(os.path.join(GAME_BasePath, "StringHashDumper_Output"), ".")

import scnlist_process

if os.path.exists(os.path.join(GAME_BasePath, "files.txt")):
    os.remove(os.path.join(GAME_BasePath, "files.txt"))
if os.path.exists(os.path.join(GAME_BasePath, "files_match.txt")):
    os.remove(os.path.join(GAME_BasePath, "files_match.txt"))
shutil.copy("files.txt", GAME_BasePath)
if os.path.exists(os.path.join(GAME_BasePath, "krkr_hxv4_dumphash.dll")):
    os.remove(os.path.join(GAME_BasePath, "krkr_hxv4_dumphash.dll"))
shutil.copy("krkr_hxv4_dumphash.dll", GAME_BasePath)
os.rename(os.path.join(GAME_BasePath, "krkr_hxv4_dumphash.dll"), os.path.join(GAME_BasePath, "version.dll"))

print("接下来将再次启动游戏，等待命令行窗口中出现了“calculate finish, results in files_match.txt, dirs_match.txt”后，关闭游戏。")
input("按回车键将开始第三步处理...")

if isLE:
    os.system(f'{LE_Path} "{GAME_Path}"')
else:
    os.system(f'"{GAME_Path}"')

input("按回车键继续。")
if isLE:
    if os.path.exists("files_match.txt"):
        os.remove("files_match.txt")
    shutil.move(os.path.join(GAME_BasePath, "files_match.txt"), ".")

import repair_name

print("处理完成。恢复了原始文件名的文件在repaired文件夹中。")