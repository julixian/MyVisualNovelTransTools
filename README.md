# MyVisualNovelTransTools
Most of them are written by ai
## AILScriptSimpleToolPlus
To dump/inject AIL engine Script from v1 to v3
v3: ??-01
v2: 02-05
v1: 06-??
## AosCompressTool
To decompress/compress .scr files to achieve No-packet-read
##BGIScriptSimpleTool
To dump/inject BGI engine Script
Most likely it won't work in very old BGI version
## EscudeScriptSimpleTool
Most likely it has no Universality for escude's scriptmode always changes
Tested on 放課後⇒エデュケーション！～先生とはじめる魅惑のレッスン～
## EscudeScriptSimpleToolV2
Most likely it has no Universality for escude's scriptmode always changes
Tested on 彗聖天使プリマヴェールZwei
## FlyingV3ArchiveTool
To extract/repack FlyingV3 .pd Archive
## GPK2CompressTool
To decompress/compress GPK2 .scb file to achieve No-packet-read
## MajiroScriptSimpleTool
To dump/inject Majiro Script .mjs(decrypted by mjcrypt) file or MajiroOBJV file(need to change file-extend-name to .mjs)
To change MajiroOBJX to MajiroOBJV You can see [GalgameReverse Project](https://github.com/YuriSizuku/GalgameReverse)
Initially made to edit script in あの晴れわたる空より高く for there are tips-jump in the game and no tools can deal with it.
So it ONLY works on new_version majirov3 script
For v1 use [VNT](https://github.com/arcusmaximus/VNTranslationTools)
For v2 and old_version v3 use [MajiroTools](https://github.com/AtomCrafty/MajiroTools)
## MyAdvArchiveTool
To extract/repack MyAdv engine .pac Archive
when packing, please limit the numbers of the files to pack or delete the config file in the dir ready to pack to prevent recompress the config file, for the config file use the different zlib compress method and if they are recompressed and replace the orgi config file in the archive, game can't run.
Tested on 彼女達は脅迫に屈する, 猫mata～猫又と兄と私の話～
## RiddleArchiveTool
To extract/repack Riddle engine .pac Archive
##RiddleCompressTool
To decompress/compress .scp file
## RiddleScriptSimpleTool
To dump/inject Riddle .scp Script file
If there are Select jump in a script, from 0x8 it has flags like Select, CngExe and F47
And 0x24、0x44、0x64…… will restore the offset
So if there are new flags, you can change the offset by hand
## TailCafArchiveTool
To extract/repack Tail engine .caf Archive
## TailScriptSimpleTool
To dump/inject Tail .scd Script file
## TopCatCompressTool
To decompress/compress TopCat engine .TCT Script file
apply for both v2 and v3
## TopTopCatV2ArchiveTool
To extract/repack TopCatV2 .TCD Archive
## TopCatV3ArchiveTool
To extract/repack TopCatV3 .TCD Archive
