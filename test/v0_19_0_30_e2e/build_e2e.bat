@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
set BOOST_ROOT=F:\b183
set FLUXING_ROOT=F:\soft\00selfmade\rime_claude
cd /d %FLUXING_ROOT%

echo === Building v0_19_0_30_e2e (binary sandbox verification) ===
echo   Target: Release/Win32

REM weasel.props hardcodes v142; this box only has VS2022 (v143).
REM Override PlatformToolset + SolutionDir via MSBuild properties (highest priority).
REM SolutionDir must be set explicitly when building a single vcxproj (not the sln),
REM otherwise $(SolutionDir) resolves to the vcxproj's own directory.
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" ^
    "%FLUXING_ROOT%\test\v0_19_0_30_e2e\v0_19_0_30_e2e.vcxproj" ^
    /t:Build ^
    /p:Configuration=Release ^
    /p:Platform=Win32 ^
    /p:PlatformToolset=v143 ^
    /p:SolutionDir=%FLUXING_ROOT%\ ^
    /p:BOOST_ROOT=F:\b183 ^
    /v:minimal ^
    /nologo

if errorlevel 1 (
  echo.
  echo BUILD FAILED with errorlevel %errorlevel%
  exit /b %errorlevel%
)

echo.
echo === Running v0_19_0_30_e2e ===
"%FLUXING_ROOT%\release\v0_19_0_30_e2e.exe"
exit /b %errorlevel%