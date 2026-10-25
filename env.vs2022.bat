rem Customize your build environment and save the modified copy to env.bat

set WEASEL_ROOT=%CD%

rem REQUIRED: path to Boost source directory
if not defined BOOST_ROOT set BOOST_ROOT=%WEASEL_ROOT%\deps\boost_1_78_0

rem OPTIONAL: architecture, Visual Studio version and platform toolset
rem set ARCH=Win32
set BJAM_TOOLSET=msvc-14.3
set CMAKE_GENERATOR="Visual Studio 17 2022"
set PLATFORM_TOOLSET=v143

rem OPTIONAL: path to additional build tools
rem set DEVTOOLS_PATH=%ProgramFiles%\Git\cmd;%ProgramFiles%\Git\usr\bin;%ProgramFiles%\CMake\bin;

rem OPTIONAL: the Rime packages bundled with the installer, fetched by plum.
rem This fork tracks output\data itself - including the fork's own
rem default.yaml with its key_binder bindings - so it bundles NO upstream
rem recipes, and build.bat therefore skips plum entirely.
rem
rem To bundle upstream packages, name them explicitly, for example:
rem   set "WEASEL_BUNDLED_RECIPES=essay"
rem Note that cmd has no way to say "defined and empty": `set "X="` DELETES
rem the variable. Leaving it unset is how "no recipes" is expressed.
