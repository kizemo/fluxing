@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
cd /D "F:\soft\00selfmade\rime_claude"
echo CWD=%CD%
echo --- listing F:\soft\00selfmade\rime_claude\env.bat ---
dir env.bat
call "F:\soft\00selfmade\rime_claude\env.bat"
echo after-call-WEASEL_BUILD=%WEASEL_BUILD%
echo after-call-PRODUCT_VERSION=%PRODUCT_VERSION%
call "F:\soft\00selfmade\rime_claude\xbuild.bat" weasel installer
set BUILD_EXIT=%errorlevel%
echo BUILD_EXIT=%BUILD_EXIT%
exit /b %BUILD_EXIT%
