@rem builds nulltest/hcfast3.dll: the release law, third form (ident_hc_rel3.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD hcfast3.c /Fe:hcfast3.dll >nul
