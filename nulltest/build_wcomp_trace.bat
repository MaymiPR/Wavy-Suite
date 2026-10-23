@echo off
rem builds wcomp_trace.exe (WComp's compressor over a file, with its meter input per millisecond)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /EHsc /std:c++17 wcomp_trace.cpp /Fe:wcomp_trace.exe >nul
