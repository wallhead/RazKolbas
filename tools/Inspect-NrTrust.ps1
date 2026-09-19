param(
    [string]$Path = "$PSScriptRoot/../SkyrimUpscalerAIOBuild16-Hotfix1/UpscalerBasePlugin/nvngx_dlssnr.dll"
)
$ErrorActionPreference = 'Stop'
$nrFile = Get-Item -LiteralPath $Path
$nrHash = (Get-FileHash -LiteralPath $nrFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$nrSignature = Get-AuthenticodeSignature -LiteralPath $nrFile.FullName
$knownCommunityBuild = ($nrHash -eq '8270b350cd82de5ce89806872cdd6b6a9249b80836b91bbeb3573470744cc206')
[ordered]@{
    path = $nrFile.FullName
    sha256 = $nrHash
    size = $nrFile.Length
    known_community_build = $knownCommunityBuild
    authenticode_status = $nrSignature.Status.ToString()
    signature_type = $nrSignature.SignatureType.ToString()
    embedded_signer = $nrSignature.SignerCertificate.Subject
    signer_valid_for_current_bytes = ($nrSignature.Status.ToString() -eq 'Valid')
    user_reported_hardware = $(if ($knownCommunityBuild) { @('RTX 20', 'RTX 30', 'RTX 40') } else { $null })
    prior_probe_observation = $(if ($knownCommunityBuild) { '2026-09-19, RTX 4080 SUPER: initialization, feature creation and retirement; evaluation NOT RUN' } else { $null })
    purpose = 'Read-only provenance diagnostic; signature presence is not authenticity of modified bytes'
} | ConvertTo-Json -Depth 4
