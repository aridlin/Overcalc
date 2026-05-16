param(
  [string]$BuildDir = "build",
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

cmake -S . -B $BuildDir
cmake --build $BuildDir --config $Config
ctest --test-dir $BuildDir --output-on-failure -C $Config

Write-Host "Windows build complete: $BuildDir/overcalc.exe"
