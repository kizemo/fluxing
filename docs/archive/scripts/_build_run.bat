@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cd /D "F:\soft\00selfmade\rime_claude"
call .\env.bat
echo === PRODUCT_VERSION=%PRODUCT_VERSION% ===
call "F:\soft\00selfmade\rime_claude\_buildflow.cmd"
set BUILD_EXIT=%errorlevel%
echo MSBUILD-EXIT=%BUILD_EXIT%
exit /b %BUILD_EXIT%
