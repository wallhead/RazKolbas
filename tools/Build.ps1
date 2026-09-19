param([ValidateSet('win-dev','win-release')][string]$Preset = 'win-dev')
$ErrorActionPreference = 'Stop'
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $installation = & $vswhere -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installation) { throw 'Visual Studio 2022 C++ Build Tools are required' }
    $env:PATH = (Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin') + ';' + $env:PATH
}
Push-Location (Join-Path $PSScriptRoot '..')
try {
    & cmake --preset $Preset
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    & cmake --build --preset $Preset
    if ($LASTEXITCODE) { throw 'CMake build failed' }
    & ctest --preset $Preset --no-tests=error --output-on-failure
    if ($LASTEXITCODE) { throw 'CTest failed' }
} finally { Pop-Location }
