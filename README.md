# MyVisualNovelTransTools
Most of them are written by AI<br/>
## AbmImageTool
To convert .png(must be 32bpp) to Lilim engine .abm image format<br/>
## ACTGSScr_oldversionArchiveTool/ACTGSScrArchiveTool
To extract/pack ACTGS engine arc.scr script Archive<br/>
Track CreateFileA that open the archive and follow few steps, a certain register will point to the key<br/>
![Image_404242458411155](https://github.com/julixian/MyVisualNovelTransTools/blob/main/images/Image_404242458411155.png)<br/>
## AdvPolaArchiveTool
Made to decompress Adv engine(Studio Polaris)『*Pola\x00 u16origlen + 3 * u8unknown + u16decompressedlen』 sign script files in まじかるカナン MAGICAL FANTASY BOX<br/>
Pack the files as a pac and use crass to unpack the pac, you can get decompressed files<br/>
For it's much easier to pack such a format than copy the decompress func from crass<br/>
## AGSIPakArchvieTool
To extract/pack AGSI engine .pak archive<br/>
## AI5WINGccImageTool
To convert .png(must be 32bpp) to Ai5Win .GCC image format(Uniformly use G24m format)<br/>
## AILScriptSimpleToolPlus/AILScriptSimpleTool++/AILScriptSimpleTool#
There are many greater tool you can try before using this outdated tool, such as [this one](https://github.com/HOKORISAMA/AILSOFT-AIL-Engine-Tools/tree/main) or [this one](https://github.com/crskycode/AIL_Tools)<br/>
To dump/inject AIL engine Script from v1 to v3<br/>
v3: ????-2001<br/>
v2: 2002-2005<br/>
v1: 2006-????<br/>
Initially, I didn't expect there would be so many types of AIL engine scripts, so I started by creating tools for each script type as I encountered them, resulting in AILScriptSimpleToolV1-V3. I then integrated them into AILScriptSimpleToolPlus through simple copy and paste. However, since I didn't have the energy to carefully analyze many OPs in V1 and V2 scripts, and the original code was written too casually to modify easily, I only added options for overwriting the original text and freely setting the overwrite starting position. This helped avoid offset overflow issues caused by u16's small maximum value and garbled text problems caused by overwriting unanalyzed sentences. While fixing some details as much as possible, this became AILScriptSimpleTool++.<br/>
<br/>
AILScriptSimpleTool# completely reconstructed the extraction and reinsertion of V1 and V2 scripts (V3 remains unchanged). I still didn't analyze their OPs, but the new tool detects which sentences in the text block weren't extracted and automatically guesses where their offsets are stored. Naturally, some sentences might be guessed incorrectly, but you can manually delete them or find the actual location storing their offsets to fix them, as the reconstructed new function only makes modifications at locations based on the offset addresses (hexadecimal numbers before ":::::") stored in the txt file.<br/>
## AnkhDatArchiveTool
To extract/repack Ankh engine .dat archive<br/>
## AoiBoxAOIBXTool
To extract/repack Aoi engine AOIBX9 format .box script resource Archive<br/>
## AoiBoxAOIMYTool
To extract/repack Aoi engine AOIMY Unicode format .box script resource Archive<br/>
when repacking pro.box, you need to manually change the index of pro.txt file to the first one in pro.box archive.<br/>
## AosCompressTool
To decompress/compress Lilim engine .scr files to achieve No-packet-read<br/>
## AZsysCompressTool
To (decrypt and decompress/compress and encrypt) AZ system engine script files<br/>
For script files in normal AZsys resource archive, you need key to decrypt/encrypt them<br/>
And there are 4 known keys in GARbro :<br/>
"Clover Heart's": 3786541434<br/>
"Triptych": 501685433<br/>
"Amaenbou": 2938115999<br/>
"Reminiscence Blue": 2849404158<br/>
You can also use guess_key func in this programme :<br/>
Use crass to unpack script.arc and make that output_dir as input_dir, the programme will use the smallest file to guess key.<br/>
If the guessed key is wrong, delete the smallest file the programme shows and try again(A file size of 1.5-3.0kb is preferred)<br/>
For script files in AZsys encrypted resource archive(need to use special parameter『system="path/to/system.arc"』 when using crass to unpack)<br/>
you needn't key, but pure file content without signature if you want to encrypt the files back<br/>
Use crass with special parameter to make it or use fixed GAR in Ellefin directory in this repository to get original encrypted files and then use the tool to make it<br/>
## AZsysCpbImageTool
To covert AZ system engine .cpb image file to bmp AND .png(must be 32bpp) to cpb file<br/>
## AZsysScriptSimpleTool
To dump/inject decompressed ASB sign .asb script files<br/>
## BananaDatPkArchiveTool
To extract/pack BANANA Shu-Shu engine .pk or .dat archive<br/>
## BGIScriptSimpleTool
To dump/inject BGI engine Script<br/>
Most likely it won't work in very old BGI version<br/>
DO NOT use it to edit config file, especially when it has messy code in the dumped txt file<br/>
## BinzArchiveTool
To extract/pack archives of [椿色のプリジオーネ](https://vndb.org/v9646)<br/>
## BndArchiveTool
To extract/pack [魔法少女アイ](https://vndb.org/v1091) .bnd archive<br/>
## CadathKarArchiveTool
To extract/pack Cadath engine KAR signature .bin Archive<br/>
## CadathSnrCryptTool
To (decrypt and decompress/compress and encrypt) Cadath engine .snr script files<br/>
Each snr file has checksum for itself like Azsystem and the checksum func is difficult to recurrent(Why not just use standard crc32 or adler32 like Azsys……)<br/>
So you need to fix game.exe to jump the check func, and [here](https://github.com/One-sixth/TsukikagerouTranslateProject/) has some fix records<br/>
## CaramelBox
A series of tools to deal with Arc3 and Arc4 engine<br/>
~~The repack function of Arc4Tool doesn't work on [ぴあ雀](https://vndb.org/v2398) and [EVE雀](https://vndb.org/v2320)~~<br/>
~~Why?~~<br/>
~~You ask me, I ask who~~<br/>
After update on 2025.08.22, you can repack them and inject their script files with parameter --compress | -c<br/>
## CrsysSceneCrpArchiveTool
To extract/pack Crsys engine(maybe this name?) scene.crp archive<br/>
## CswareDL1ArchiveTool
To extract/repack Csware .DL1 Archive<br/>
## CVNSCpz2ArchiveTool
To pack a cpz2 Archive<br/>
have no compress AND md5_compute function in the programme<br/>
so you have to modify game.exe to jump the md5_check(track MessageBox) and decompress_function([here](https://youtu.be/xgpYNJMMb6o/) is the method)<br/>
## EAGLSAdvsysArchiveTool
To extract/repack EAGLS .pak Advsys type encrypted Archive<br/>
## EAGLSDecryptTool
To decrypt/encrypt EAGLS .dat EAGLS type encrypted script files to achieve No-packet-read.<br/>
## Ellefin
Release-EPK:Fixed GAR To extract some games of Terios made by Ellefin engine<br/>
EPKscpro.py:To decrypt script files of はぴベルラヴ×2ハネムーン<br/>
## EntisDecryptTool
To extract EntisGLS engine .noa archive<br/>
Initially made to extract some script.csx in script.noa because GARbro doesn't work on them<br/>
To find key, you need to use ResourceHacker(or any other PE tool) to save the IDR_COTOMI of game.exe as a binary file, like this:<br/>
![Snipaste_2025-07-07_23-32-40](https://github.com/julixian/MyVisualNovelTransTools/blob/main/images/Snipaste_2025-07-07_23-32-40.png)<br/>
And then use [ericomp.exe](https://github.com/satan53x/SExtractor/tree/main/tools/EntisGLS) to decompress the binary file to xml, the key will be stored there<br/>
## EscudeScriptSimpleTool
Most likely it has no Universality because escude's scriptmode always changes<br/>
Tested on 放課後⇒エデュケーション！～先生とはじめる魅惑のレッスン～<br/>
## EscudeScriptSimpleToolV2
Most likely it has no Universality because escude's scriptmode always changes<br/>
Tested on 彗聖天使プリマヴェールZwei<br/>
## FlyingV3ArchiveTool
To extract/repack Flying ShineV3 .pd Archive<br/>
## FosterFA2ArchiveTool
To extract/repack Foster game engine .FA2 Archive<br/>
It will set all compression flags to 0(no compression) so the repacked archive looks like plaintext<br/>
## FrontWingPacArchiveTool
To extract/repack FrontWing engine LIB_PACKDATA0000 sign .pac(ArcFLT in GARbro) Archive<br/>
## G2CryptTool
To dump/inject G2 engine .g2 script files<br/>
## GPK2CompressTool
To decompress/compress GPK2 .scb file to achieve No-packet-read<br/>
## GsWin2ArchiveTool && GsWin2ImageTool
To extract/pack GsWin2 .pak archive<br/>
To convert gspic2png and png2gspic<br/>
If you want to edit script files without truncation, trying to see [this project](https://github.com/icey9527/yami)<br/>
## GsWin4ArchiveTool
To extract/pack GsWin4 .pak archive<br/>
For GsWin5, use AnimED to repack<br/>
## IceArchiveTool
To extract/repack IceSoft .BIN Archive<br/>
## IceCompressTool
To decompress/compress(Actually just add a TPW\x00 sign) IceSoft TPW sign Script files<br/>
## IvoryTool
A serial of tools to extract/repack .pk archive, decrypt/encrypt script files, dump/inject texts from/into script files<br/>
## LambdaCLSArchiveTool && LambdaCLSImageTool
To extract/repack Lambda(maybe?) engine .DAT CLSLINK Archive<br/>
To convert clspic2png and png2clspic<br/>
The sturct of the CLS archive seems simple, but I failed to create readable archives<br/>
if I create them directly from directory instead of adding files to the end of the original archive<br/>
Maybe it has some hard-codes or strange alignment rules? Who knows, I just use the way that would work<br/>
## LambdaLapArchiveTool
To extract/repack Lambda engine gsce.lap Archive<br/>
## LambdaLapArchiveSimpleTool && LambdaCompressTool
To extract Lambda engine gevent.lap image archive<br/>
you need to use archivetool to extract compressed files and use compresstool to decompress files twice(how foolish...)<br/>
## LambdaLAXArchiveTool
To extract/repack Lambda engine .lax Archive<br/>
Uniformly use lzss fake compression to repack<br/>
## LazycrewScriptSimpleTool
To dump/inject Lazycrew script.dat file<br/>
I haven't analyzed jump op or chunck length, so it needs truncation now<br/>
## LucifenSobScriptSimpleTool
To dump/inject Lucifen/Ellefin engine .sob script files<br/>
For tob, use Ineditor or [jyxjyx1234's tool](https://github.com/jyxjyx1234/misc_game-chs/tree/re_upload/%E7%8C%AB%E6%92%AB%E3%83%87%E3%82%A3%E3%82%B9%E3%83%88%E3%83%BC%E3%82%B7%E3%83%A7%E3%83%B3)(need to fix HScene script by padding the file to the same size)<br/>
## MainProgramHoepDatCryptTool
To decrypt/encrypt MainProgramHoep engine .dat script file<br/>
## MajiroScriptSimpleTool
To dump/inject Majiro Script .mjs(decrypted by mjcrypt) file or MajiroOBJV file(need to change file-extend-name to .mjs)<br/>
To change MajiroOBJX to MajiroOBJV You can see [GalgameReverse Project](https://github.com/YuriSizuku/GalgameReverse)<br/>
Initially made to edit script in あの晴れわたる空より高く for there are tips-jump in the game and no tools can deal with it.<br/>
So it ONLY works on few new_version majirov3 script(Looking back at this tool a year later, I feel it was written so foolishly...)<br/>
For v1 use [VNT](https://github.com/arcusmaximus/VNTranslationTools)<br/>
For v2 and old_version v3 use [MajiroTools](https://github.com/AtomCrafty/MajiroTools) or Ineditor<br/>
## MasysScriptSimpleTool
To dump/inject Masys engine .meg script files<br/>
Search "Powered" to find key in game.exe<br/>
Example:<br/>
![Snipaste_2025-02-16_01-21-26.png](https://github.com/julixian/MyVisualNovelTransTools/blob/main/images/Snipaste_2025-02-16_01-21-26.png)<br/>
I haven't analyzed jump op or chunck length, so it needs truncation now.
## MarbleMblArchiveTool
To extract/repack Marble engine mg_data(%d).mbl Archive<br/>
If the game has no key(can be correctly extracted by the default method in GARbro), keep key empty<br/>
Or you can use known keys in GARbro<br/>
![Snipaste_2025-01-30_15-13-54](https://github.com/julixian/MyVisualNovelTransTools/blob/main/images/Snipaste_2025-01-30_15-13-54.png)
You can also manually find the key in game.exe, [here](https://github.com/shangjiaxuan/Crass-source/blob/4aff113b98fc39fb85f64501ab47c580df779a3d/cui/MarbleEngine/MarbleEngine.cpp) exists the method to find key<br/>
For example:<br/>
![Snipaste_2025-01-30_14-25-37](https://github.com/julixian/MyVisualNovelTransTools/blob/main/images/Snipaste_2025-01-30_14-25-37.png)
![Snipaste_2025-01-30_15-21-02](https://github.com/julixian/MyVisualNovelTransTools/blob/main/images/Snipaste_2025-01-30_15-21-02.png)
the key of 彼女が見舞いに来ない理由 is 0x46554A4953415741 and Cafe AQUA is 0x89B482CD97598EF782BE
## MnpMmaArchiveTool
To extract/repack Mnp Engine .mma archive(only works on script archive now, because pictures are stored in a special way in mma and the tool doesn't process it)<br/>
## MTSPakZArchiveTool
To extract/repack MTS engine .pak or .z Archive<br/>
## MyAdvArchiveTool
To extract/repack MyAdv engine .pac Archive<br/>
when packing, please limit the numbers of the files to pack or delete the config files in the dir ready to pack to prevent replacing the config file in the archive, for the config file use different zlib compress method and if they are recompressed and replace the orgi config file in the archive, game can't run.<br/>
Tested on 彼女達は脅迫に屈する, 猫mata～猫又と兄と私の話～<br/>
## NextonLikeCLstArchiveTool
To extract/repack Nexton LikeC(LC-ScriptEngine) archive with .lst index file<br/>
Support both Moon and Nexton archive type<br/>
## NextonLikeCScriptSimpleTool
To dump/inject Nexton LikeC(LC-ScriptEngine) .SNX script file<br/>
## OhgetsuPacArchiveTool
To extract/repack Ohgetsu engine script.pac Archive<br/>
when repacking, the programm will set the compression flag to 0(no compression) in exe<br/>
So do not compress the files back before repacking<br/>
You can also use crass to unpack the archive(using special parameter 『exe="path/to/your/game.exe"』, will decompress the files at the same time)<br/>
## OtemotoCompressTool
To decompress/fake compress the lzss compressed files<br/>
## OtemotoTLZArchiveTool
To extract/repack Otemoto engine .TLZ Archive<br/>
## RiddleArchiveTool
To extract/repack Riddle engine .pac Archive<br/>
## RiddleCompressTool
To decompress/compress Riddle .scp file<br/>
## RiddleScriptSimpleTool
To dump/inject Riddle .scp Script file<br/>
If there are Select jump in a script, from 0x8 it has flags like Select, CngExe and F47<br/>
And 0x24、0x44、0x64…… will store the offset<br/>
So if there are new flags, you can change the offset by hand<br/>
## RMTImageTool
To convert .png(must be 32bpp) to AI5WIN/AI6WIN .rmt image format<br/>
## RPMArchiveTool
To extract/pack RPM/ZENOS engine .arc archvie<br/>
## SAS5OldScriptSimpleTool
To dump/inject texts in old(From version 100000 to 104000) SAS5 engine CODE section(in .sec5 file)<br/>
For higher version, use [SAS5Tool](https://github.com/pkuislm/SAS5Tool)<br/>
For extract/repack sec5 archive, use [sec5_unpack/sec5_pack](https://github.com/Yggdrasill-Moe/Niflheim/tree/master/StudioSeldomAdventureSystem)<br/>
## ScoopFxArchiveTool
To extract/pack Scoop engine .FX archive<br/>
## SeraphScriptArchiveTool
To extract/pack Seraphim engine ScnPac.Dat script archive<br/>
## TacticsScriptSimpleTool
To dump/inject Tactics .bin script files<br/>
## TailCafArchiveTool
To extract/repack Tail engine .caf Archive<br/>
## TailScriptSimpleTool
To dump/inject Tail .scd Script file<br/>
## TopCatCompressTool
To decompress/compress TopCat engine .TCT Script file<br/>
apply for both v2 and v3<br/>
## TopCatV2ArchiveTool
To extract/repack TopCatV2 .TCD Archive<br/>
## TopCatV3ArchiveTool
To extract/repack TopCatV3 .TCD Archive<br/>
## ValkArchiveTool
A serial of tools to extract latest dat archive(2025.08), extract/pack am2 multi-frame archive format, convert image format between mg2 and png<br/>
## ValkDataImageTool
To convert .png(must be 32bpp) to Valkyria engine data\d format image in odn archive(only works on few games now)<br/>
## WscScriptSimpleTool
To dump/inject Willplus V1 engine .wsc script files<br/>
## YaneuraoDatArchiveTool
To extract/repack Yaneurao engine .dat Archive<br/>
## YaneuraoYgaImageTool
To convert .yga image to bmp AND .bmp to yga(no compress) image<br/>
If want to edit the image, convert the image to png first. And before converting to yga, convert the png back to bmp and then convert to yga.<br/>
Or the image will lose Transparent pixel.<br/>
## YoxDatArchvieTool
To extract/repack YoxAdv+++ engine .dat archive<br/>
v1: Tested on かぎろひ～勺景～<br/>
v2: Tested on MUSICUS!<br/>
## YoxScriptSimpleTool/YoxScriptSimpleToolV2
To dump/inject YoxAdv+++ engine script files<br/>
## misc_pack
To pack [this engine](https://github.com/Dir-A/GARbro/blob/master/ArcFormats/Misc/ArcBIN.cs) ([凌辱学園～部活調教恥獄責め～](https://vndb.org/v7127))<br/>
You need to download python 3.11 to use it.
