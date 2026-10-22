@rem builds nulltest/hcgrad.dll: the release law, sixth form (ident_hc_rel4.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD hcgrad.c /Fe:hcgrad.dll >nul
