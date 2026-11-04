param([switch]$CoreOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) { $cmake = $cmakeCommand.Source }
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 Build Tools: Desktop development with C++, CMake, Windows SDK.' }
    $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake not found. Enable C++ CMake tools in Visual Studio Installer.' }
$buildName = if ($CoreOnly) { 'build-core' } else { 'build' }
$gui = if ($CoreOnly) { 'OFF' } else { 'ON' }
if (-not $CoreOnly -and -not (Test-Path -LiteralPath 'third_party/JUCE/CMakeLists.txt')) {
    git clone --depth 1 --branch 9.0.5 https://github.com/juce-framework/JUCE.git third_party/JUCE
    if ($LASTEXITCODE -ne 0) { throw 'JUCE download failed' }
}
if (-not $CoreOnly) {
    $revision = git -C third_party/JUCE rev-parse HEAD
    if ($revision -ne 'd60b7a78030661574a615b586d9325ba7a7fa7dd') { throw 'Unexpected JUCE revision. See docs/TECHNOLOGY.md.' }
}
& $cmake -S . -B $buildName -A x64 "-DDRUMPILOT_GUI=$gui"
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
& $cmake --build $buildName --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
& $ctest --test-dir $buildName -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
