param(
    [Parameter(Mandatory)][string]$Destination,
    [ValidateSet('win-dev','win-release')][string]$Preset = 'win-release',
    [string]$NvidiaSrRuntime,
    [string]$IniSource
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$configuration = if ($Preset -eq 'win-dev') { 'Debug' } else { 'Release' }
$binary = Join-Path $root "build/$Preset/bin/$configuration/RazKolbas.dll"
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Build product missing: $binary" }
$destinationPath = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $destinationPath) { throw 'Destination must be new; refusing to overwrite a mod or game installation' }
$ini = if ($IniSource) { [IO.Path]::GetFullPath($IniSource) } else { Join-Path $root 'config/RazKolbas.ini.example' }
if ($IniSource -and -not (Test-Path -LiteralPath $ini -PathType Leaf)) { throw 'Specified INI source is missing' }
if ($NvidiaSrRuntime) {
    $runtimeFile = [IO.Path]::GetFullPath($NvidiaSrRuntime)
    if (-not (Test-Path -LiteralPath $runtimeFile -PathType Leaf)) { throw 'NVIDIA SR runtime is missing' }
    $runtimeHash = (Get-FileHash -LiteralPath $runtimeFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($runtimeHash -ne 'c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e') { throw 'NVIDIA SR runtime hash differs' }
    if ((Get-AuthenticodeSignature -LiteralPath $runtimeFile).Status -ne 'Valid') { throw 'NVIDIA SR runtime signature is not valid' }
}
$plugins = Join-Path $destinationPath 'SKSE/Plugins'
New-Item -ItemType Directory -Path $plugins -Force | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $plugins 'RazKolbas.dll')
if (Test-Path -LiteralPath $ini) { Copy-Item -LiteralPath $ini -Destination (Join-Path $plugins 'RazKolbas.ini') }
if ($NvidiaSrRuntime) {
    $runtimeTarget = Join-Path $plugins 'RazKolbasRuntime'
    New-Item -ItemType Directory -Path $runtimeTarget -Force | Out-Null
    Copy-Item -LiteralPath $runtimeFile -Destination (Join-Path $runtimeTarget 'nvngx_dlss.dll')
}
$imguiNotice = Join-Path $root 'licenses/DearImGui-MIT.txt'
if (-not (Test-Path -LiteralPath $imguiNotice -PathType Leaf)) { throw 'Dear ImGui MIT notice is missing' }
$licenses = Join-Path $destinationPath 'LICENSES'
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
Copy-Item -LiteralPath $imguiNotice -Destination (Join-Path $licenses 'DearImGui-MIT.txt')
$destinationPrefix = $destinationPath.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
$files = @(Get-ChildItem -LiteralPath $destinationPath -File -Recurse | ForEach-Object {
    if (-not $_.FullName.StartsWith($destinationPrefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Staged file escaped destination' }
    @{ path=$_.FullName.Substring($destinationPrefix.Length).Replace('\','/'); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
@{ product='RazKolbas'; status=$(if ($NvidiaSrRuntime) { 'EXPERIMENTAL_GUARDED_DLSS_SR' } else { 'DEVELOPMENT_RENDERER_OBSERVER_OPT_IN' }); files=$files; uninstall='Remove only listed files whose hashes still match, or remove this isolated MO2 mod folder.' } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destinationPath 'install-manifest.json') -Encoding utf8
Write-Output $destinationPath
