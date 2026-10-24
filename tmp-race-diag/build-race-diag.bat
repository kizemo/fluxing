@echo off
rem build-race-diag.bat — compile the instrumented spec 076 race repro
rem Boost libs mirror test\TestPipeChannelRace\TestPipeChannelRace.vcxproj:
rem   /MT (static CRT) + the "-s-" boost variants + the serialization lib dir.
setlocal enableextensions
call "C:\PROGRA~2\MICROS~2\2022\BUILDT~1\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
if errorlevel 1 ( echo [ERROR] vcvars32 failed & exit /b 1 )
set "BLO=F:\b183\bin.v2\libs\serialization\build\msvc-14.3\release\address-model-32\link-static\runtime-link-static\threading-multi"
cd /d "F:\soft\00selfmade\rime_claude"
cl /nologo /EHsc /std:c++17 /MT /O2 /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE ^
   /I"F:\soft\00selfmade\rime_claude\include" /I"F:\b183" ^
   /Fo"F:\soft\00selfmade\rime_claude\tmp-race-diag\\" ^
   /Fe"F:\soft\00selfmade\rime_claude\tmp-race-diag\RaceDiag.exe" ^
   "F:\soft\00selfmade\rime_claude\tmp-race-diag\RaceDiag.cpp" ^
   "F:\soft\00selfmade\rime_claude\WeaselIPC\PipeChannel.cpp" ^
   /link /LIBPATH:"%BLO%" libboost_wserialization-vc143-mt-s-x32-1_83.lib libboost_serialization-vc143-mt-s-x32-1_83.lib
exit /b %ERRORLEVEL%
