@rem builds nulltest/mbfeat.dll (the WBassX training feature helpers, train_wbassx.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD mbfeat.c /Fe:mbfeat.dll >nul
del mbfeat.exp mbfeat.lib mbfeat.obj 2>nul
