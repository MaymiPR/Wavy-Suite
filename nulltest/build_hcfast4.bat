@rem builds nulltest/hcfast4.dll: the release law, fourth form (ident_hc_rel4.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD hcfast4.c /Fe:hcfast4.dll >nul
