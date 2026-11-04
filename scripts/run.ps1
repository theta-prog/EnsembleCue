$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $projectRoot 'build\DrumPilot_artefacts\Release\DrumPilot.exe'
if (-not (Test-Path -LiteralPath $exe)) { & (Join-Path $PSScriptRoot 'build.ps1') }
# This is the interactive application the user requested, not a background helper.
Start-Process -FilePath $exe -WorkingDirectory $projectRoot
