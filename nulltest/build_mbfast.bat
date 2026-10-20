@rem builds nulltest/mbfast.dll: the WBassX generator loop and its exact linearisation (mbmodel.py, extract_g3.py, train_wbassx.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD mbfast.c /Fe:mbfast.dll >nul
