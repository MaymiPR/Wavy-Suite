@rem builds nulltest/hcfast6.dll: the release law, sixth form (ident_hc_rel4.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD hcfast6.c /Fe:hcfast6.dll >nul
