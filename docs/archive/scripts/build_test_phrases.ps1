$env:BOOST_ROOT='F:\b183'
$env:BJAM_TOOLSET='msvc-14.3'
$env:PLATFORM_TOOLSET='v143'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin;' + $env:PATH
Set-Location 'F:\soft\00selfmade\rime_claude'
msbuild 'test\TestPhrasesDialog\TestPhrasesDialog.vcxproj' /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:BOOST_ROOT=F:\b183 /p:SolutionDir=F:\soft\00selfmade\rime_claude\ /v:minimal /nologo 2>&1 | Tee-Object -FilePath 'build_test.log' | Select-Object -Last 30
Write-Host "===ExitCode: $LASTEXITCODE==="