param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [Parameter(Mandatory)][string]$ReShadeDll,
    [Parameter(Mandatory)][string]$EnbDll,
    [string]$SteamOverlayDll = '-',
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$probe = Join-Path $repository 'build/win-release/bin/Release/RazKolbasFgPrivateGameRouteProbe.exe'
if (-not (Test-Path -LiteralPath $probe)) { throw "Probe is not built: $probe" }
$isolatedRoot = [System.IO.Path]::GetFullPath((Join-Path $repository 'artifacts/local')) + [System.IO.Path]::DirectorySeparatorChar
$reshadePath = (Resolve-Path -LiteralPath $ReShadeDll).Path
if (-not $reshadePath.StartsWith($isolatedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'ReShade DLL must be a copy beneath ignored artifacts/local so the installed preset remains untouched'
}

$lines = & $probe $RuntimeDirectory $ReShadeDll $SteamOverlayDll $EnbDll 'effect-trace' 2>&1
$result = $LASTEXITCODE
$text = ($lines | Out-String)
if ($OutputPath) {
    $parent = Split-Path -Parent $OutputPath
    if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    [System.IO.File]::WriteAllText($OutputPath, $text)
}
Write-Output $text
if ($result -ne 0) { throw "Effect probe exited $result" }
if ($text -notmatch 'Effect trace registered=1') {
    throw 'ReShade public effect-event registration was not observed'
}
$runtimes = @($lines | Select-String '^Effect trace runtime=')
if ($text -notmatch 'Effect trace registered=1 runtimes=\d+ overflow=0') {
    throw 'Effect trace runtime records overflowed'
}
foreach ($epoch in 0, 1) {
    $executing = @($runtimes | Where-Object {
        $_.Line -match " epoch=$epoch " -and $_.Line -match ' techniques=[1-9]\d*'
    })
    if ($executing.Count -lt 2) {
        throw "Expected two runtimes executing techniques in epoch $epoch; got $($executing.Count)"
    }
    if ($text -notmatch "Effect trace coexecuted epoch=$epoch frames=[1-9]\d*") {
        throw "No same-frame effect execution across runtimes in epoch $epoch"
    }
}
