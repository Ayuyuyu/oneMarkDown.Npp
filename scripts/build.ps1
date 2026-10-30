param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build'

if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
    cmake -S $root -B $buildDir -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
}

cmake --build $buildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }

$dll = Join-Path $root "bin\OneMarkDownNpp.dll"
if (Test-Path $dll) {
    Write-Host "Build succeeded: $dll" -ForegroundColor Green
} else {
    throw "Output DLL not found: $dll"
}
