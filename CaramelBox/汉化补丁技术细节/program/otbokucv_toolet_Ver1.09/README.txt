Changelog:
2006.10.29 by 痴汉公贼	补丁程序修正了系统菜单等提示信息的乱码
2006.10.28 by 痴汉公贼	1.08发布，增加了封包器1.0
2006.10.25 by 痴汉公贼	1.07发布，otoboku_cn.exe去掉了“メディア倫理協会認可”的提示（废除）
2006.10.19 by 痴汉公贼	1.06发布，包含文本写回程序1.1, 修正了写回偏移值的bug；增加汉化版程序；增加临时用写回程序
2006.10.16 by 痴汉公贼	1.05发布，增加scb文本写回程序1.0
2006.10.11 by 痴汉公贼	1.04发布，包含解包器1.5, 修正了缺少导出文件的bug
2006.10.11 by 痴汉公贼	1.03发布，包含解包器1.4, 支持解压lz压缩数据
2006.10.05 by 痴汉公贼	1.02发布，包含解包器1.3, 支持.$$$后缀文件的明确的输出
2006.10.02 by 痴汉公贼	1.01发布，包含解包器1.2, 修正了文件名的bug，支持mode2格式
2006.10.01 by 痴汉公贼	1.00发布，包含解包器1.1
			
			処女はお姉さまに恋してる 工具集

【文件列表】
66d1e56babf47e262345b96c7bf6c68a	【tools\otbokucv_bin_unpacker.exe】
解包器程序，用于提取bin文件的内部资源.

2a33b7bf082cb781ac2fb1f84622706a	【tools\otbokucv_bin_packer.exe】
封包器程序，用于重制作bin文件.

c52be01520d26640768671a35cb1ad5e	【tools\otbukocv_text_writeback.exe】
文本写回程序.

1e0177ec0d12044fa6351fe576a5f222	【patch\otoboku_cn.exe】
汉化过的主程序。该程序是基于升级补丁1.2制作的。

【责任和义务】
0. 运行前请务必使用winmd5程序验证所有文件的校验和，以免遭到恶意程序的攻击.
1. 本程序由痴汉公贼一人独立完成.
2. 这些程序只由痴汉公贼本人对核心人员或经过授权的人员发布.
3. 获得封包程序的人员未经痴汉公贼本人允许,不得私自对外发布此工具,否则后果自负!
4. 请自行承担该程序的运行风险. 任何恶劣后果请自行承担.
5. 这些程序仅在简体中文winxp sp2下测试通过.

【解包程序使用说明】
0. 运行前请务必使用winmd5程序验证otbokucv_bin_unpacker.exe的正确性，以免遭到恶意程序的攻击.
1. 将otbokucv_bin_unpacker.exe文件复制到游戏目录下（确保和其他的bin文件在同一目录下）.
2. 开始->运行->输入cmd.
3. 在命令行中顺序输入（假设你的游戏安装在w:\処女はお姉さまに恋してる）: 

	w:\
	cd "w:\処女はお姉さまに恋してる"
	dir

   dir命令执行正确的话应该可以看到该目录下的otbokucv_bin_unpacker.exe文件.
4. 执行(这里仅以setup.bin文件为例,其他文件请对号入座): otbokucv_bin_unpacker.exe setup.bin
5. 运行时会快速滚动类似以下的字样:

	pngmenu07.png: extracting OK
	pngmenu08.png: type 0, length 5305, offset 8a800
	pngmenu08.png: extracting OK
	pngmenu09.png: type 0, length 5979, offset 8c000
	pngmenu09.png: extracting OK
	setup.bin: extract OK

   当程序执行完毕后, otbokucv_bin_unpacker.exe所在的目录下会产生setup.bin内的所有资源文件.

【文本写回工具使用说明】
一.准备阶段:
1. 创建目录otbokucv_work, 然后再在其中创建3个目录: OUTPUT_dir，TEXT_dir和SCB_dir目录。

2. 将tools\otbukocv_text_writeback.exe复制到otbokucv_work目录下.

3. 将游戏解包器解出的所有scb文件（封装在system.bin内部）都复制到otbokucv_work\SCB_dir目录下。

二.使用阶段
1. 将修稿人员修正过的scb稿子放在otbokucv_work\TEXT_dir目录下.

2. 在cmd中执行otbukocv_text_writeback.exe:

Product:        処女はお姉さまに恋してる
Packge:         scb
Program:        writeback
Author:         痴汉公贼
Revision:       1.0
Date:           2006.10.16
Usage:          writeback.exe
OUTPUT_dir\scb1n.scb: Write back file 3855 lines ...

（这个例子仅以scb1n.scb一个文件为例）以上结果为正确结果.如果发生错误,请联络痴汉公贼.

3. 最终的scb文件在otbokucv_work\OUTPUT_dir目录下.

【封包器使用说明】
1. 将tools\otbokucv_bin_packer.exe文件复制到w:\処女はお姉さまに恋してる目录下。

2. 启动cmd，进入3days_works目录下。

3. 确保otbokucv_bin_packer.exe和已经用解包器的解开的资源放在同一个目录中（文本是用writeback重写回过的scb）。

4. 执行以下2条命令（这里以system.bin为例）: 

	otbokucv_bin_unpacker.exe system.bin -i

	otbokucv_bin_packer.exe system_tmp.bin system.bin.idx

运行时会快速滚动类似以下的字样:

	oggbgm21.ogg: packing ...
	oggbgm22.ogg: packing ...
	oggbgm23.ogg: packing ...
	oggbgm24.ogg: packing ...

最后结果正确地话应当是：

	system_new.bin: pack file OK

   当程序执行完毕后, 当前目录下会产生新封装好的system_new.bin文件，将该文件改名为system.bin覆盖游戏安装目录下的同名文件即可（注意备份）。


