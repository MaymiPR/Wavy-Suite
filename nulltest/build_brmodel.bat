@rem builds nulltest/brmodel.dll (WBass Rider's riding model, driven by brmodel.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /fp:fast /D_USE_MATH_DEFINES /LD brmodel.c /Fe:brmodel.dll >nul
