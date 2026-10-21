@rem builds nulltest/hcfast.dll: H-Comp's identified gain path per sample (hcmodel.py, ident_hc_*.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD hcfast.c /Fe:hcfast.dll >nul
