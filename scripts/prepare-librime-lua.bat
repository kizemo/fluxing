@echo off
rem prepare-librime-lua.bat
rem Copies the vendored librime-lua source (thirdparty/librime-lua) into
rem librime/plugins/lua so the librime CMake build auto-discovers it.
rem This avoids needing librime-lua to be a git submodule while keeping
rem the librime submodule clean.
rem
rem Idempotent: safe to run multiple times.
rem
rem Spec 005.6: required for rime_ice lua_processor/lua_translator/lua_filter
rem to work at runtime.

setlocal

if not exist thirdparty\librime-lua (
  echo Error: thirdparty\librime-lua\ not found. Run scripts\fetch-librime-lua.bat first.
  exit /b 1
)

if not exist thirdparty\librime-lua\thirdparty\lua5.4\lua.h (
  echo Error: thirdparty\librime-lua\thirdparty\lua5.4\lua.h not found.
  echo Run scripts\fetch-librime-lua-thirdparty.bat first.
  exit /b 1
)

if not exist librime\plugins mkdir librime\plugins
if exist librime\plugins\lua rmdir /s /q librime\plugins\lua

echo Copying thirdparty\librime-lua ^-^> librime\plugins\lua ...
xcopy /E /I /Y /Q thirdparty\librime-lua librime\plugins\lua > nul
if errorlevel 1 (
  echo Error: failed to copy librime-lua source.
  exit /b 1
)
echo Done.
endlocal
