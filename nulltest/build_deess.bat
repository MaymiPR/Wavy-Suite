@rem builds nulltest/deess.dll (the de-esser model for fit_voxchain_deess.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD deess.c /Fe:deess.dll >nul
