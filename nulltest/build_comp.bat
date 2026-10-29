@rem builds nulltest/comp.dll (the Compress model for fit_voxchain_compress.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD comp.c /Fe:comp.dll >nul
