@rem builds nulltest/vrmodel.dll (WVox Rider's riding model, driven by vrmodel.py)
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /fp:fast /D_USE_MATH_DEFINES /LD vrmodel.c /Fe:vrmodel.dll >nul
