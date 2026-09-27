param(
    [Parameter(Mandatory)][string]$Destination,
    [ValidateSet('win-dev','win-release')][string]$Preset = 'win-release',
    [string]$NvidiaSrRuntime,
    [string]$NvidiaNrRuntime,
    [hashtable]$NrRuntimeProfiles = @{},
    [string]$IniSource
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$configuration = if ($Preset -eq 'win-dev') { 'Debug' } else { 'Release' }
$hasSrRuntime = -not [string]::IsNullOrWhiteSpace($NvidiaSrRuntime)
$hasNrRuntime = -not [string]::IsNullOrWhiteSpace($NvidiaNrRuntime) -or $NrRuntimeProfiles.Count -gt 0
if ($hasNrRuntime -and -not $hasSrRuntime) {
    throw 'The current pre-SR NR bridge requires the pinned SR runtime in the same package'
}
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
if ($NvidiaNrRuntime) {
    $nrRuntimeFile = [IO.Path]::GetFullPath($NvidiaNrRuntime)
    if (-not (Test-Path -LiteralPath $nrRuntimeFile -PathType Leaf)) { throw 'DLSS-NR runtime is missing' }
    $nrItem = Get-Item -LiteralPath $nrRuntimeFile
    $nrHash = (Get-FileHash -LiteralPath $nrRuntimeFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($nrItem.Length -ne 165840496 -or $nrHash -ne '91ea4143d9ed1cb90b11a2851cfc68dabe7d1e7414f8dfaa8016d86b99e40be7') {
        throw 'Fast-FP16 DLSS-NR runtime identity differs'
    }
    if ($nrItem.VersionInfo.FileVersion -ne '310,8,0,0') { throw 'DLSS-NR runtime version differs' }
    $nrSignature = Get-AuthenticodeSignature -LiteralPath $nrRuntimeFile
    if ($nrSignature.Status -ne 'HashMismatch' -or
        -not $nrSignature.SignerCertificate -or
        $nrSignature.SignerCertificate.Subject -notmatch 'NVIDIA Corporation') {
        throw 'DLSS-NR runtime does not have the expected modified NVIDIA provenance'
    }
}
$nrCatalog = @{
    'plain-fp16-20-30' = @{ size=309671536; hash='6dac1b40f0c87af84a8177b18c741e84fb0c914f204c9d87d95916b665ba3af8'; signature='HashMismatch' }
    'ada-fastfp16' = @{ size=165840496; hash='e67dee209320cdafe0e93e45675d7aa34323a53acc57a72b2e40a181581c989a'; signature='HashMismatch' }
    'nvidia-50' = @{ size=165840496; hash='e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e'; signature='Valid' }
}
foreach ($profileId in $NrRuntimeProfiles.Keys) {
    if (-not $nrCatalog.ContainsKey($profileId)) { throw "Unknown NR runtime profile: $profileId" }
    $source = [IO.Path]::GetFullPath([string]$NrRuntimeProfiles[$profileId])
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "NR runtime missing: $profileId" }
    $expected = $nrCatalog[$profileId]
    $item = Get-Item -LiteralPath $source
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($item.Length -ne $expected.size -or $hash -ne $expected.hash) {
        throw "NR runtime identity differs: $profileId"
    }
    if ($item.VersionInfo.FileVersion -ne '310,8,0,0') { throw "NR runtime version differs: $profileId" }
    $signature = Get-AuthenticodeSignature -LiteralPath $source
    if ([string]$signature.Status -ne $expected.signature -or
        -not $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch 'NVIDIA Corporation') {
        throw "NR runtime provenance differs: $profileId"
    }
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
if ($NvidiaNrRuntime) {
    $runtimeTarget = Join-Path $plugins 'RazKolbasRuntime'
    New-Item -ItemType Directory -Path $runtimeTarget -Force | Out-Null
    Copy-Item -LiteralPath $nrRuntimeFile -Destination (Join-Path $runtimeTarget 'nvngx_dlssnr.dll')
}
foreach ($profileId in $NrRuntimeProfiles.Keys) {
    $runtimeTarget = Join-Path $plugins "RazKolbasRuntime/NR/$profileId"
    New-Item -ItemType Directory -Path $runtimeTarget -Force | Out-Null
    Copy-Item -LiteralPath ([IO.Path]::GetFullPath([string]$NrRuntimeProfiles[$profileId])) -Destination (Join-Path $runtimeTarget 'nvngx_dlssnr.dll')
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
@{ product='RazKolbas'; status=$(if ($hasNrRuntime) { 'EXPERIMENTAL_GUARDED_DLSS_SR_NR' } elseif ($NvidiaSrRuntime) { 'EXPERIMENTAL_GUARDED_DLSS_SR' } else { 'DEVELOPMENT_RENDERER_OBSERVER_OPT_IN' }); files=$files; uninstall='Remove only listed files whose hashes still match, or remove this isolated MO2 mod folder.' } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destinationPath 'install-manifest.json') -Encoding utf8
Write-Output $destinationPath
