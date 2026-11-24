@echo off
rem Builds sets.exe, the stock voice through Din Sync's 25 sets, optimised. Run it in a folder holding out\:
rem   sets.exe [A-E]   one row of sets, or all; then convert.py, then server.py.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /O2 /Ob2 /std:c++20 /EHsc /DNDEBUG /I ..\..\external\coolsynth-engine\src sets.cpp /Fe:%TEMP%\sets.exe /Fo:%TEMP%\sets.obj
