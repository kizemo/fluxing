@echo off

setlocal

rem L145: the version of this build comes from env.bat if the developer put
rem one there, and from the fork's own defaults below otherwise. Clear what
rem the environment inherited first: a machine-level environment can carry
rem leftovers from another build (this machine still has 0.19.0 leftovers
rem from the 0.19.x era), and an inherited value must not beat the defaults.
rem That is exactly how CI ended up building a 0.17.4 product. env.bat is
rem called after this block and can still override every one of them.
set "FLUXING_VERSION="
set "VERSION_MAJOR="
set "VERSION_MINOR="
set "VERSION_PATCH="
set "WEASEL_BUILD="
set "PRODUCT_VERSION="
set "FILE_VERSION="
set "RELEASE_BUILD="

if not exist env.bat copy env.bat.template env.bat

if exist env.bat call env.bat

if not defined WEASEL_ROOT set WEASEL_ROOT=%CD%

rem L145: this fork's own version line. Upstream keeps its release line in
rem these three numbers and bumps them at every release; the fork inherited
rem the mechanism but never the numbers, so a build with no version in
rem env.bat (CI) fell back to upstream's 0.17.4 - and the non-release tag
rem lookup below then matched UPSTREAM's 0.17.4 tag and counted the commits
rem since it. The fork's own release tags are v<major>.<minor>.<patch>.<build>
rem (currently v0.21.0.6), so the lookup now resolves to the fork's line.
rem Bump these three numbers next to the CHANGELOG entry and the git tag.
if not defined VERSION_MAJOR set VERSION_MAJOR=0
if not defined VERSION_MINOR set VERSION_MINOR=21
if not defined VERSION_PATCH set VERSION_PATCH=0

if not defined FLUXING_VERSION set FLUXING_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%
if not defined WEASEL_BUILD set WEASEL_BUILD=0

rem use numeric build version for release build
set PRODUCT_VERSION=%FLUXING_VERSION%.%WEASEL_BUILD%
rem for non-release build, try to use git commit hash as product build version
if not defined RELEASE_BUILD (
  rem check if git is installed and available, then get the short commit id of head
  git --version >nul 2>&1
  if not errorlevel 1 (
    for /f "delims=" %%i in ('git tag --sort=-creatordate ^| findstr /r "%FLUXING_VERSION%"') do (
      set LAST_TAG=%%i
      goto found_tag
    )
    :found_tag
    for /f "delims=" %%i in ('git rev-list %LAST_TAG%..HEAD --count') do (
      set WEASEL_BUILD=%%i
    )
    rem get short commmit id of head
    for /F %%i in ('git rev-parse --short HEAD') do (set PRODUCT_VERSION=%FLUXING_VERSION%.%WEASEL_BUILD%.%%i)
  )
)

rem FILE_VERSION is always 4 numbers; same as PRODUCT_VERSION in release build
if not defined FILE_VERSION set FILE_VERSION=%FLUXING_VERSION%.%WEASEL_BUILD%

echo PRODUCT_VERSION=%PRODUCT_VERSION%
echo FLUXING_VERSION=%FLUXING_VERSION%
echo WEASEL_BUILD=%WEASEL_BUILD%
echo WEASEL_ROOT=%WEASEL_ROOT%
echo WEASEL_BUNDLED_RECIPES=%WEASEL_BUNDLED_RECIPES%
echo.

if defined GITHUB_ENV (
	setlocal enabledelayedexpansion
	echo git_ref_name=%PRODUCT_VERSION%>>!GITHUB_ENV!
)

if defined BOOST_ROOT (
  if exist "%BOOST_ROOT%\boost" goto boost_found
)
echo Error: Boost not found! Please set BOOST_ROOT in env.bat.
exit /b 1

:boost_found
echo BOOST_ROOT=%BOOST_ROOT%
echo.

if not defined BJAM_TOOLSET (
  rem the number actually means platform toolset, not %VisualStudioVersion%
  set BJAM_TOOLSET=msvc-14.2
)

if not defined PLATFORM_TOOLSET (
  set PLATFORM_TOOLSET=v142
)

rem L139: every *.vcxproj in this tree - the Weasel* projects AND all the
rem test projects - imports ..\..\weasel.props, which is a GENERATED file
rem (.gitignore lists weasel.props) that only the Weasel branch used to
rem render. So `build.bat rime` - exactly what the CI test job runs - left
rem it missing, and every test project then failed to load with "the
rem imported project ... was not found". Render it here instead: it is cheap,
rem and it makes the props file a property of the build rather than a side
rem effect of one branch.
call :render_props
if errorlevel 1 goto error

if defined DEVTOOLS_PATH set PATH=%DEVTOOLS_PATH%%PATH%

set build_config=Release
set build_option=/t:Build
set build_boost=0
set boost_build_variant=release
set build_data=0
set build_opencc=0
set build_rime=0
set rime_build_variant=release
set build_weasel=0
set build_installer=0
set build_arm64=0

rem parse the command line options
:parse_cmdline_options
  if "%1" == "" goto end_parsing_cmdline_options
  if "%1" == "debug" (
    set build_config=Debug
    set boost_build_variant=debug
    set rime_build_variant=debug
  )
  if "%1" == "release" (
    set build_config=Release
    set boost_build_variant=release
    set rime_build_variant=release
  )
  if "%1" == "rebuild" set build_option=/t:Rebuild
  if "%1" == "boost" set build_boost=1
  if "%1" == "data" set build_data=1
  if "%1" == "opencc" set build_opencc=1
  if "%1" == "rime" set build_rime=1
  if "%1" == "librime" set build_rime=1
  if "%1" == "weasel" set build_weasel=1
  if "%1" == "installer" set build_installer=1
  if "%1" == "arm64" set build_arm64=1
  if "%1" == "all" (
    set build_boost=1
    set build_data=1
    set build_opencc=1
    set build_rime=1
    set build_weasel=1
    set build_installer=1
    set build_arm64=1
  )
  shift
  goto parse_cmdline_options
:end_parsing_cmdline_options

if %build_weasel% == 0 (
if %build_boost% == 0 (
if %build_data% == 0 (
if %build_opencc% == 0 (
if %build_rime% == 0 (
  set build_weasel=1
)))))

rem quit WeaselServer.exe before building
cd /d %WEASEL_ROOT%
if exist output\weaselserver.exe (
  output\weaselserver.exe /q
)

rem build booost
if %build_boost% == 1 (
  call :build_boost
  if errorlevel 1 exit /b 1
  cd /d %WEASEL_ROOT%
)

rem -------------------------------------------------------------------------
rem build librime x64 and Win32
if %build_rime% == 1 (
  if not exist librime\build.bat (
    git submodule update --init --recursive
  )
  rem L133: the deps of the librime submodule (deps/opencc, deps/yaml-cpp,
  rem deps/marisa-trie, deps/leveldb, deps/glog) are submodules of librime
  rem ITSELF. actions/checkout with "submodules: true" runs "git submodule
  rem update --init --force" WITHOUT --recursive, so on CI the librime checkout
  rem arrives with an empty deps\ directory and the opencc patch below dies
  rem with "opencc CMakeLists.txt not found". The guard above only runs the
  rem submodule update when librime\build.bat is missing, which is never the
  rem case on CI. So check for the nested deps as well.
  if not exist librime\deps\opencc\CMakeLists.txt (
    git submodule update --init --recursive
  )
  call scripts\prepare-librime-lua.bat
  if errorlevel 1 exit /b 1
  rem L121: deps/opencc needs a Windows build patch. find_library() cannot
  rem see the bundled marisa-trie (it is a CMake target, not a marisa.lib on
  rem disk), so LIBMARISA ends up empty and the link step fails. This patch
  rem used to exist ONLY as an unrecorded hand-edit inside the librime
  rem submodule, so a fresh clone could not build librime at all, and any
  rem "git checkout ." inside librime silently broke the build. It is now
  rem applied from the main repo, so the submodule itself stays clean.
  powershell -ExecutionPolicy Bypass -File scripts\patch-librime-opencc-marisa.ps1
  if errorlevel 1 exit /b 1
  rem L135: the clean-cache loop below uses LIBRIME-relative paths
  rem (build, dist, lib, deps\*\build), so it has to run inside librime\.
  rem Upstream rime/weasel does `cd %WEASEL_ROOT%\librime` right here (see
  rem upstream commit 135b50a). This fork's commit face243 dropped that cd
  rem while integrating librime-lua, and nothing ever restored it. The
  rem consequence was silent and severe:
  rem   - the loop resolved against the REPO root and deleted
  rem     %WEASEL_ROOT%\lib\ -- the directory get-rime.ps1 installs rime.lib
  rem     into, and one of the Weasel* projects' link search paths;
  rem   - the later `copy ...\dist_Win32\lib\rime.lib %WEASEL_ROOT%\lib\`
  rem     then failed with "The system cannot find the path specified";
  rem   - and the failure was swallowed (see L135 note on `call :label`
  rem     below), so `build.bat rime` still exited 0 and CI reported a
  rem     GREEN step that had produced nothing. The real symptom showed up
  rem     two steps later as "LNK1181: cannot open input file 'rime.lib'".
  cd /d "%WEASEL_ROOT%\librime"
  if errorlevel 1 (
    echo Error: cannot enter %WEASEL_ROOT%\librime -- is the submodule checked out?
    exit /b 1
  )
  rem clean cache before building
  for %%a in ( build dist lib ^
    deps\glog\build ^
    deps\googletest\build ^
    deps\leveldb\build ^
    deps\marisa-trie\build ^
    deps\opencc\build ^
    deps\yaml-cpp\build ) do (
      if exist %%a rd /s /q %%a
  )
  cd /d "%WEASEL_ROOT%"

  rem build x64 librime
  set ARCH=x64
  call :build_librime_platform x64 %WEASEL_ROOT%\lib64 %WEASEL_ROOT%\output
  rem L135: a failing `call :label` subroutine must not be swallowed. A
  rem `goto error` inside a subroutine only exits THAT subroutine, so the
  rem caller kept going and the script exited 0 -- a green step that had
  rem in fact produced nothing. Same for the Win32 pass below.
  if errorlevel 1 goto error
  rem build Win32 librime
  set ARCH=Win32
  call :build_librime_platform Win32 %WEASEL_ROOT%\lib %WEASEL_ROOT%\output\Win32
  if errorlevel 1 goto error
  rem L121: revert the opencc patch so the librime submodule is left in its
  rem upstream state. The patch is re-applied automatically on the next build
  rem (the script is idempotent), so reverting here is safe and keeps the
  rem submodule pointer clean. See lessons-learned.md L121.
  rem L140: this runs with the current directory still set to librime\ (the
  rem :build_librime_platform calls leave it there), so the relative path did
  rem not resolve and the revert silently did nothing:
  rem   The argument 'scripts\patch-librime-opencc-marisa.ps1' to the -File
  rem   parameter does not exist.
  rem The error was printed and then swallowed, so the librime submodule stayed
  rem patched - and L121 warns that a later `git checkout` inside librime
  rem would then break the build. Use an absolute path, and do not swallow
  rem the failure.
  powershell -ExecutionPolicy Bypass -File "%WEASEL_ROOT%\scripts\patch-librime-opencc-marisa.ps1" -Revert
  if errorlevel 1 goto error
)

rem -------------------------------------------------------------------------
if %build_weasel% == 1 (
  if not exist output\data\essay.txt (
    set build_data=1
  )
  if not exist output\data\opencc\TSCharacters.ocd* (
    set build_opencc=1
  )
)
rem L135: same swallowed-errorlevel trap as the librime passes above --
rem a failing subroutine must not let the script continue and exit 0.
if %build_data% == 1 (
  call :build_data
  if errorlevel 1 goto error
)
if %build_opencc% == 1 (
  call :build_opencc_data
  if errorlevel 1 goto error
)

if %build_weasel% == 0 goto end

cd /d %WEASEL_ROOT%

rem L128: the Weasel* projects link yaml-cpp.lib out of the librime deps
rem build tree.  A fresh clone does not have it, and the failure otherwise
rem shows up as a cryptic "LNK1181: cannot open input file yaml-cpp.lib"
rem deep inside the msbuild output.  Say what to run instead.
if not exist librime\deps\yaml-cpp\build_Win32\Release\yaml-cpp.lib (
  echo Error: the librime dependency libraries are not built yet.
  echo        Run "build.bat rime" first. It builds librime and its deps
  echo        for Win32 and x64 - yaml-cpp, opencc, marisa-trie, leveldb, glog.
  exit /b 1
)

call :render_props
if errorlevel 1 goto error

del msbuild*.log

if defined SDKVER set build_sdk_option=/p:WindowsTargetPlatformVersion=%SDKVER%
if not defined SDKVER set build_sdk_option=

if %build_arm64% == 1 (

  msbuild.exe weasel.sln %build_option% /p:Configuration=%build_config% /p:Platform="ARM" /fl6 %build_sdk_option%
  if errorlevel 1 goto error
  msbuild.exe weasel.sln %build_option% /p:Configuration=%build_config% /p:Platform="ARM64" /fl5 %build_sdk_option%
  if errorlevel 1 goto error
)

rem L128: the x64 platform must NOT build the whole solution. librime in this
rem fork is Win32-only (lib64/rime.lib is machine 14C), so the x64
rem WeaselServer/WeaselDeployer can never link -- and neither of them is
rem shipped: the installer only carries weaselx64.dll as an x64 file.
rem Building them anyway makes `build.bat weasel installer` fail on a step
rem that does not produce anything we distribute.
rem WeaselTSF does not link rime at all, so it builds for x64 on its own.
rem NOTE: building a .vcxproj outside the solution leaves $(SolutionDir)
rem undefined, and every include/library path in this tree is
rem $(SolutionDir)-relative -> pass it explicitly.
msbuild.exe WeaselTSF\WeaselTSF.vcxproj %build_option% /p:Configuration=%build_config% /p:Platform="x64" /p:SolutionDir=%WEASEL_ROOT%\ /fl2 %build_sdk_option%
if errorlevel 1 goto error
msbuild.exe weasel.sln %build_option% /p:Configuration=%build_config% /p:Platform="Win32" /fl1 %build_sdk_option%
if errorlevel 1 goto error

if %build_arm64% == 1 (
  pushd arm64x_wrapper
  call build.bat
  if errorlevel 1 goto error
  popd

  copy arm64x_wrapper\weaselARM64X.dll output
  if errorlevel 1 goto error
)

if %build_installer% == 1 (
  rem L141: stage the text assets the installer embeds before makensis
  rem runs, so this target does not depend on a previous "build.bat data".
  call :stage_text_assets
  if errorlevel 1 goto error
  rem L144: the installer is written to output\archives\ (install.nsi:51
  rem writes OutFile "archives\fluxing-...-installer.exe", relative to the
  rem script directory), and .gitignore:27 excludes that directory - so a
  rem clean checkout has no output\archives at all, and NSIS does not
  rem create the subdirectory for a relative OutFile. It aborted with
  rem   Output: "...\output\archives\fluxing-...-installer.exe"
  rem   Can't open output file
  rem The maintainer's machine has the directory because previous installers
  rem sit in it; a clean checkout does not. Create it before packaging.
  if not exist "%WEASEL_ROOT%\output\archives" mkdir "%WEASEL_ROOT%\output\archives"
  if errorlevel 1 goto error
  "%ProgramFiles(x86)%"\NSIS\Bin\makensis.exe ^
  /DFLUXING_VERSION=%FLUXING_VERSION% ^
  /DWEASEL_BUILD=%WEASEL_BUILD% ^
  /DPRODUCT_VERSION=%PRODUCT_VERSION% ^
  output\install.nsi
  if errorlevel 1 goto error
)

goto end

rem -------------------------------------------------------------------------
rem build boost
:build_boost
  set BJAM_OPTIONS_COMMON=-j%NUMBER_OF_PROCESSORS%^
    --with-filesystem^
    --with-json^
    --with-locale^
    --with-regex^
    --with-serialization^
    --with-system^
    --with-thread^
    define=BOOST_USE_WINAPI_VERSION=0x0603^
    toolset=%BJAM_TOOLSET%^
    link=static^
    runtime-link=static^
    --build-type=complete
  
  set BJAM_OPTIONS_X86=%BJAM_OPTIONS_COMMON%^
    architecture=x86^
    address-model=32
  
  set BJAM_OPTIONS_X64=%BJAM_OPTIONS_COMMON%^
    architecture=x86^
    address-model=64
  
  set BJAM_OPTIONS_ARM32=%BJAM_OPTIONS_COMMON%^
    define=BOOST_USE_WINAPI_VERSION=0x0A00^
    architecture=arm^
    address-model=32
  
  set BJAM_OPTIONS_ARM64=%BJAM_OPTIONS_COMMON%^
    define=BOOST_USE_WINAPI_VERSION=0x0A00^
    architecture=arm^
    address-model=64
  
  cd /d %BOOST_ROOT%
  if not exist b2.exe call bootstrap.bat
  if errorlevel 1 goto error
  b2 %BJAM_OPTIONS_X86% stage %BOOST_COMPILED_LIBS%
  if errorlevel 1 goto error
  b2 %BJAM_OPTIONS_X64% stage %BOOST_COMPILED_LIBS%
  if errorlevel 1 goto error
  
  if %build_arm64% == 1 (
    b2 %BJAM_OPTIONS_ARM32% stage %BOOST_COMPILED_LIBS%
    if errorlevel 1 goto error
    b2 %BJAM_OPTIONS_ARM64% stage %BOOST_COMPILED_LIBS%
    if errorlevel 1 goto error
  )
  exit /b

rem ---------------------------------------------------------------------------
rem ---------------------------------------------------------------------------
:render_props
  rem weasel.props is imported by every *.vcxproj through a path relative to
  rem the project file, so it has to be rendered into the repository root no
  rem matter what the current directory is.
  pushd "%WEASEL_ROOT%"
  set WEASEL_PROJECT_PROPERTIES=BOOST_ROOT^
    PLATFORM_TOOLSET^
    VERSION_MAJOR^
    VERSION_MINOR^
    VERSION_PATCH^
    PRODUCT_VERSION^
    FILE_VERSION
  cscript.exe render.js weasel.props %WEASEL_PROJECT_PROPERTIES%
  set "PROPS_RC=%ERRORLEVEL%"
  popd
  if not "%PROPS_RC%"=="0" goto error
  exit /b

rem ---------------------------------------------------------------------------
:stage_text_assets
  rem L141: output\install.nsi resolves its relative paths against the SCRIPT
  rem directory - makensis chdirs into the script's directory unless /NOCD is
  rem given. Measured with a three-case probe: with LICENSE.txt next to the
  rem script it compiles, with /NOCD it fails with the very same
  rem "LicenseData: open failed" message the CI runner produced, and with the
  rem file only in the CWD plus /NOCD it compiles again. So the installer needs
  rem these three text assets INSIDE output\:
  rem   LICENSE.txt       install.nsi:72 (MUI_PAGE_LICENSE) and :565 (File)
  rem   README.txt        install.nsi:566 (File)
  rem   rime-install.bat  install.nsi:574 (File)
  rem All three are gitignored build products (.gitignore:25 "/output/*.txt").
  rem Commit 41e47e3 deleted output\LICENSE.txt from tracking and added that
  rem ignore rule; :build_data recreated the other two but never this one, so a
  rem clean checkout had no output\LICENSE.txt and makensis aborted with
  rem   LicenseData: open failed "LICENSE.txt"
  rem Stage all three here. /Y so a second run cannot stop at the overwrite
  rem prompt, and the exit code is checked so a failed copy cannot be
  rem swallowed (the L135 pattern).
  if not exist "%WEASEL_ROOT%\LICENSE.txt" (
    echo Error: %WEASEL_ROOT%\LICENSE.txt is missing.
    exit /b 1
  )
  if not exist "%WEASEL_ROOT%\README.md" (
    echo Error: %WEASEL_ROOT%\README.md is missing.
    exit /b 1
  )
  if not exist "%WEASEL_ROOT%\plum\rime-install.bat" (
    echo Error: %WEASEL_ROOT%\plum\rime-install.bat is missing.
    exit /b 1
  )
  copy /Y "%WEASEL_ROOT%\LICENSE.txt" "%WEASEL_ROOT%\output\LICENSE.txt"
  if errorlevel 1 exit /b 1
  copy /Y "%WEASEL_ROOT%\README.md" "%WEASEL_ROOT%\output\README.txt"
  if errorlevel 1 exit /b 1
  copy /Y "%WEASEL_ROOT%\plum\rime-install.bat" "%WEASEL_ROOT%\output\rime-install.bat"
  if errorlevel 1 exit /b 1
  exit /b

:build_data
  call :stage_text_assets
  if errorlevel 1 goto error
  rem L138: this fork TRACKS output\data itself - 139 files, including the
  rem fork's own default.yaml with its key_binder bindings (Control+Shift+9,
  rem Control+Shift+numbersign, Shift+space, ...). plum resolves the
  rem "prelude" package to "default.yaml symbols.yaml"
  rem (plum\scripts\split-packages.sh:21), so a bare
  rem `bash plum/rime-install` OVERWRITES the fork's tracked default.yaml
  rem with the upstream rime one. That silently did two kinds of damage:
  rem   1. the deployer aborted with "missing input schema: quick5", because
  rem      the upstream schema_list lists schemes this fork does not ship;
  rem   2. the packaged product lost the fork's key_binder bindings.
  rem WEASEL_BUNDLED_RECIPES was echoed by build.bat but never DEFINED
  rem anywhere in the repository, so every build silently took that path.
  rem
  rem The rule from now on: plum only ever runs with an EXPLICIT list.
  rem   - the variable unset or empty: skip plum, output\data is used
  rem     exactly as tracked (this is what this fork wants);
  rem   - the variable set to a list: run plum with exactly that list.
  rem Note that cmd cannot express "defined and empty" - `set "X="` DELETES
  rem the variable - which is why the rule has only these two states.
  if not defined WEASEL_BUNDLED_RECIPES goto skip_plum
  if "%WEASEL_BUNDLED_RECIPES%"=="" goto skip_plum
  rem L138 safety net: plum writes straight into output\data. Snapshot the
  rem git status of that directory before the fetch, compare after: if the
  rem fetch touched ANY file there - modified, added or deleted - then the
  rem data we are about to package is not the data in git. Abort instead of
  rem shipping the difference. The comparison is against the snapshot, not
  rem against "clean", so a pre-existing dirty tree does not false-alarm.
  set "DATA_STATUS_BEFORE=%TEMP%\fluxing-data-status-before.txt"
  set "DATA_STATUS_AFTER=%TEMP%\fluxing-data-status-after.txt"
  if exist "%WEASEL_ROOT%\.git" git -C "%WEASEL_ROOT%" status --porcelain -- output/data > "%DATA_STATUS_BEFORE%"
  set plum_dir=plum
  set rime_dir=output/data
  set WSLENV=plum_dir:rime_dir
  bash plum/rime-install %WEASEL_BUNDLED_RECIPES%
  if errorlevel 1 goto error
  if not exist "%WEASEL_ROOT%\.git" goto data_fetch_checked
  git -C "%WEASEL_ROOT%" status --porcelain -- output/data > "%DATA_STATUS_AFTER%"
  fc /b "%DATA_STATUS_BEFORE%" "%DATA_STATUS_AFTER%" >nul
  if errorlevel 1 goto data_fetch_dirty
:data_fetch_checked
  exit /b

:data_fetch_dirty
  echo [ERROR] the plum fetch modified files under output\data.
  echo         The packaged data would differ from the repository.
  echo         See lessons-learned.md L138.
  goto error

:skip_plum
  echo [INFO] WEASEL_BUNDLED_RECIPES is not set - skipping the plum package fetch.
  echo        output\data is tracked in git and is used as-is.
  exit /b

rem ---------------------------------------------------------------------------
:build_opencc_data
  if not exist %WEASEL_ROOT%\librime\share\opencc\TSCharacters.ocd2 (
    cd %WEASEL_ROOT%\librime
    call build.bat deps %rime_build_variant%
    if errorlevel 1 goto error
  )
  cd %WEASEL_ROOT%
  if not exist output\data\opencc mkdir output\data\opencc
  copy %WEASEL_ROOT%\librime\share\opencc\*.* output\data\opencc\
  if errorlevel 1 goto error
  exit /b

rem ---------------------------------------------------------------------------
rem %1 : ARCH
rem %2 : push | pop , push to backup when pop to restore
:stash_build
  pushd %WEASEL_ROOT%\librime
  for %%a in ( build dist lib ^
    deps\glog\build ^
    deps\googletest\build ^
    deps\leveldb\build ^
    deps\marisa-trie\build ^
    deps\opencc\build ^
    deps\yaml-cpp\build ) do (
    if "%2"=="push" (
      if exist %%a  move %%a %%a_%1 
    )
    if "%2"=="pop" (
      if exist %%a_%1  move %%a_%1 %%a 
    )
  )
  popd
  exit /b

rem ---------------------------------------------------------------------------
rem %1 : ARCH
rem %2 : target_path of rime.lib, base %WEASEL_ROOT% or abs path
rem %3 : target_path of rime.dll, base %WEASEL_ROOT% or abs path
:build_librime_platform
  rem restore backuped %1 build
  call :stash_build %1 pop

  cd %WEASEL_ROOT%\librime
  if not exist env.bat (
    copy %WEASEL_ROOT%\env.bat env.bat
  )
  if not exist lib\opencc.lib (
    call build.bat deps %rime_build_variant%
    if errorlevel 1 (
      call :stash_build %1 push
      goto error
    )
  )
  call build.bat %rime_build_variant%
  if errorlevel 1 (
    call :stash_build %1 push
    goto error
  )

  cd %WEASEL_ROOT%\librime
  call :stash_build %1 push

  copy /Y %WEASEL_ROOT%\librime\build_%1\src\Release\rime.lib %WEASEL_ROOT%\librime\dist_%1\lib\rime.lib
  if errorlevel 1 goto error

  copy /Y %WEASEL_ROOT%\librime\dist_%1\include\rime_*.h %WEASEL_ROOT%\include\
  if errorlevel 1 goto error
  copy /Y %WEASEL_ROOT%\librime\dist_%1\lib\rime.lib %2\
  if errorlevel 1 goto error
  copy /Y %WEASEL_ROOT%\librime\dist_%1\lib\rime.dll %3\
  if errorlevel 1 goto error

  exit /b
rem ---------------------------------------------------------------------------

:error

cd %WEASEL_ROOT%
echo error building weasel...
exit /b 1

:end
cd %WEASEL_ROOT%
