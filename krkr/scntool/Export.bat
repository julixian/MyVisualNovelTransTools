@echo off
if not exist scn md scn
if not exist txt md txt
if not exist new_scn md new_scn
if not exist new_txt md new_txt
set /p getnum=Please input language index(0~n):
echo Exporting json...
for %%a in (scn\*.scn) do start /min "PsbDecompile" "tools\PsbDecompile.exe" "%%a"
:fwait
set running=0
tasklist | find /i "PsbDecompile.exe" >nul && set running=1
if %running%==1 goto fwait
echo Exporting txt...
python "tools\export.py" %getnum%
echo Done.
pause