@rem builds nulltest/hcfast7.dll: the release law, seventh form (ident_hc_rel4.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD hcfast7.c /Fe:hcfast7.dll >nul
