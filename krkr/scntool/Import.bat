@echo off
set /p getnum=Please input language index(0~n):
set /p append=Add as new language?(0/1):
echo Exporting json...
for %%a in (scn\*.scn) do start /min "PsbDecompile" "tools\PsbDecompile.exe" "%%a"
:fwait
set running=0
tasklist | find /i "PsbDecompile.exe" >nul && set running=1
if %running%==1 goto fwait
echo Importing txt...
python "tools\import.py" %getnum% %append% | find /i "Error" && goto fend
echo Importing json...
for %%a in (scn\*.scn) do start /min "PsBuild" "tools\PsBuild.exe" "scn\%%~na.json"
:fwait2
set running=0
tasklist | find /i "PsBuild.exe" >nul && set running=1
if %running%==1 goto fwait2
echo Parsing scn...
for %%a in (scn\*.scn) do (
 findstr /m "mdf" "%%a">nul && "tools\mdftool.exe" "%%~na.pure.scn">nul
 move "%%~na.pure.scn" "new_scn\%%~na.scn">nul
)
echo Done.
:fend
pause