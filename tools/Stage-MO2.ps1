param(
    [Parameter(Mandatory)][string]$Destination,
    [ValidateSet('win-dev','win-release')][string]$Preset = 'win-release'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$configuration = if ($Preset -eq 'win-dev') { 'Debug' } else { 'Release' }
$binary = Join-Path $root "build/$Preset/bin/$configuration/RazKolbas.dll"
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Build product missing: $binary" }
$destinationPath = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $destinationPath) { throw 'Destination must be new; refusing to overwrite a mod or game installation' }
$plugins = Join-Path $destinationPath 'SKSE/Plugins'
New-Item -ItemType Directory -Path $plugins -Force | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $plugins 'RazKolbas.dll')
$ini = Join-Path $root 'config/RazKolbas.ini.example'
if (Test-Path -LiteralPath $ini) { Copy-Item -LiteralPath $ini -Destination (Join-Path $plugins 'RazKolbas.ini') }
$files = @(Get-ChildItem -LiteralPath $destinationPath -File -Recurse | ForEach-Object {
    @{ path=[IO.Path]::GetRelativePath($destinationPath,$_.FullName).Replace('\','/'); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
@{ product='RazKolbas'; status='DEVELOPMENT_RENDERER_OBSERVER_OPT_IN'; files=$files; uninstall='Remove only listed files whose hashes still match, or remove this isolated MO2 mod folder.' } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destinationPath 'install-manifest.json') -Encoding utf8
Write-Output $destinationPath
