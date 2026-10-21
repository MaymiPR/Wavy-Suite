@rem builds nulltest/hcfast2.dll: the two-state release law (ident_hc_release2.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD hcfast2.c /Fe:hcfast2.dll >nul
