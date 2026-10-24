@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cd /D "F:\soft\00selfmade\rime_claude"
call .\env.bat
echo === NSIS FLUXING_VERSION=%FLUXING_VERSION% WEASEL_BUILD=%WEASEL_BUILD% ===
call "F:\soft\00selfmade\rime_claude\_nsis_only.cmd"
set BUILD_EXIT=%errorlevel%
echo NSIS-EXIT=%BUILD_EXIT%
dir "F:\soft\00selfmade\rime_claude\output\archives\fluxing-%FLUXING_VERSION%.%WEASEL_BUILD%-installer.exe"
exit /b %BUILD_EXIT%
