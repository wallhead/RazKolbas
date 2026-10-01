param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [Parameter(Mandatory)][string]$ReShadeDll,
    [Parameter(Mandatory)][string]$EnbDll,
    [string]$SteamOverlayDll = '-',
    [ValidateSet('colour-trace', 'colour-single-d3d11')][string]$Mode = 'colour-trace',
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$probe = Join-Path $repository 'build/win-release/bin/Release/RazKolbasFgPrivateGameRouteProbe.exe'
if (-not (Test-Path -LiteralPath $probe)) { throw "Probe is not built: $probe" }
$isolatedRoot = [System.IO.Path]::GetFullPath((Join-Path $repository 'artifacts/local')) + [System.IO.Path]::DirectorySeparatorChar
$reshadePath = (Resolve-Path -LiteralPath $ReShadeDll).Path
if (-not $reshadePath.StartsWith($isolatedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'ReShade DLL must be a copy beneath ignored artifacts/local'
}

$lines = & $probe $RuntimeDirectory $ReShadeDll $SteamOverlayDll $EnbDll $Mode 2>&1
$result = $LASTEXITCODE
$text = ($lines | Out-String)
if ($OutputPath) {
    $parent = Split-Path -Parent $OutputPath
    if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    [System.IO.File]::WriteAllText($OutputPath, $text)
}
Write-Output $text
if ($result -ne 0) { throw "Colour probe exited $result" }
foreach ($epoch in 0, 1) {
    $frame = 100 + 120 * $epoch
    $samples = @($lines | Select-String "^Colour trace epoch=$epoch frame=$frame sample=(TL|TR|BL|BR) source=\d+,\d+,\d+,255 d3d11=\d+,\d+,\d+,\d+ d3d12=\d+,\d+,\d+,\d+$")
    if ($samples.Count -ne 4) {
        throw "Expected four ordered midtone samples for epoch $epoch; got $($samples.Count)"
    }
    if ($text -notmatch "Colour stage epoch=$epoch frame=$frame sourceHash=[0-9a-f]+ d3d11Hash=[0-9a-f]+ d3d12Hash=[0-9a-f]+ fullEqual=[01]") {
        throw "Expected a whole-buffer D3D11/D3D12 comparison for epoch $epoch"
    }
    if ($text -notmatch "Effect screenshot epoch=$epoch frame=$frame succeeded=1 size=\d+x\d+ hash=[0-9a-f]+") {
        throw "Expected a final D3D12 ReShade screenshot for epoch $epoch"
    }
    if ($Mode -eq 'colour-single-d3d11' -and
        $text -notmatch "Effect trace runtime=[^\r\n]+ api=0xc000 epoch=$epoch [^\r\n]+ techniques=0 finishes=") {
        throw "Expected no D3D12 ReShade techniques for epoch $epoch"
    }
    if ($Mode -eq 'colour-single-d3d11' -and
        $text -notmatch "Effect trace runtime=[^\r\n]+ api=0xb000 epoch=$epoch [^\r\n]+ techniques=[1-9]\d* finishes=") {
        throw "Expected active D3D11 ReShade techniques for epoch $epoch"
    }
    if ($Mode -eq 'colour-single-d3d11' -and
        $text -notmatch "Effect trace coexecuted epoch=$epoch frames=0") {
        throw "Expected zero double-effect frames for epoch $epoch"
    }
}
if ($Mode -eq 'colour-single-d3d11' -and
    $text -notmatch 'Effect suppression D3D12 init=2 failures=0') {
    throw 'Expected successful startup and resize suppression'
}
if ($Mode -eq 'colour-single-d3d11' -and
    $text -notmatch 'Effect suppression direct-toggle reasserted=1') {
    throw 'Expected the lower runtime to reject direct re-enablement before rendering'
}
