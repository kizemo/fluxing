@echo off
rem fetch-librime-lua.bat
rem Clones hchunhui/librime-lua master into thirdparty/librime-lua\.
rem Run once after clone, or to refresh the vendored source.
rem
rem Requires: git, internet (or HTTP_PROXY / HTTPS_PROXY env)

setlocal

if exist thirdparty\librime-lua (
  echo thirdparty\librime-lua\ already exists, skipping clone.
  echo To refresh: rmdir /s /q thirdparty\librime-lua and re-run.
  exit /b 0
)

if not exist thirdparty mkdir thirdparty

echo Fetching hchunhui/librime-lua master ...
git clone --depth 1 https://github.com/hchunhui/librime-lua.git thirdparty\librime-lua
if errorlevel 1 exit /b 1
echo Done. Now run scripts\fetch-librime-lua-thirdparty.bat to fetch the lua5.4 source.
endlocal
