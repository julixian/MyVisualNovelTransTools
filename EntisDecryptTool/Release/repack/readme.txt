
Rosetta


■ はじめに
　Rosetta をダウンロードくださいましてありがとうございます。
　Rosetta は EntisGLS4 で提供されるスクリプト言語で、ゲーム中のスクリプト処理の他、テキスト、画像、音声、動画などのデータを簡易に処理することを目的とした汎用的なインタプリタです。
　使用は自己責任でお願いいたします。



■ 使用方法
　コマンドラインから以下のコマンドを実行すると書式を表示できます。

> rosetta /?


　以下のようにしてスクリプトを実行できます。（/arg 以下は任意）

> rosetta script.rs /arg arg0 arg1 ...


　環境変数 ROSETTA_INCLUDE_PATH にインポートソースパスを設定しておくと、/I で指定しなくてもディレクトリを省略して import できます。
　デフォルトでは lib と tools を追加しておくことを推奨します。



■ サンプル
　samples 以下にサンプルスクリプトをいくつか同梱しています。
　samples 以下 .bat ファイルで各種サンプルの起動、ソースファイルは samples\src にあります。
　00_hello_window, 01_sprite_action のように数字の順に見ていくとわかりやすいよう考慮されています。



■ デバッグモード
　/debug オプションを伴って起動すると、ラインデバッガでデバッグ実行できます。
　デバッグ実行では JIT コンパイラは無効です。

　デバッグ中のコマンド一覧は

>?

で表示できます。

command usage;
g                   : run debug
p                   : trace (step over)
t [<steps>]         : trace (step in)
l [<first> [<end>]] : source lines
cs <filename>       : switch current source
bp                  : list all break points
bp set <line>       : set break point
bp reset <line>     : reset break point
d [local]           : dump local variables
d global            : dump global variables
d this              : dump this members
? <expression>      : evaluate expression

*push ESC key to stop execution.



■ 連絡先
http://www.entis.jp/
support@entis.jp


