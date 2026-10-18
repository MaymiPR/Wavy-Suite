@rem builds nulltest/fastdsp.dll (attack/release follower used by wbass_model.py; optional, a Python fallback exists)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /LD fastdsp.c /Fe:fastdsp.dll >nul
