param([int]$DeviceSeconds=60,[switch]$Asio)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
New-Item -ItemType Directory -Force results | Out-Null
& .\build\Release\core_tests.exe | Tee-Object results/core-tests.txt
if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
& .\build\Release\sample_rate_tests.exe | Tee-Object results/sample-rate-tests.txt
if ($LASTEXITCODE -ne 0) { throw 'Sample rate tests failed' }
& .\build\Release\quality_benchmark.exe | Tee-Object results/quality.csv
if ($LASTEXITCODE -ne 0) { throw 'Quality evaluation failed' }
& .\build\Release\benchmark.exe 1800 | Tee-Object results/benchmark-1800s.csv
if ($LASTEXITCODE -ne 0) { throw 'Benchmark failed' }
& .\build\drum_render_artefacts\Release\drum_render.exe --demo "$PWD\results\demo-band.wav" 30
if ($LASTEXITCODE -ne 0) { throw 'Rendering failed' }
& .\build\drum_render_artefacts\Release\drum_render.exe "$PWD\results\demo-band-drums.wav" "$PWD\results\wav-replay.wav"
if ($LASTEXITCODE -ne 0) { throw 'WAV replay failed' }
$probeArguments = @($DeviceSeconds)
if ($Asio) { $probeArguments += '--asio' }
& .\build\audio_probe_artefacts\Release\audio_probe.exe @probeArguments | Tee-Object results/audio-probe-summary.txt
if ($LASTEXITCODE -ne 0) { throw 'Device probe failed or did not lock. Check the report.' }
