param(
    [string]$Path = "$PSScriptRoot/../artifacts/local/nr-fastfp16/nvngx_dlssnr.dll"
)
$ErrorActionPreference = 'Stop'
$nrFile = Get-Item -LiteralPath $Path
$nrHash = (Get-FileHash -LiteralPath $nrFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$nrSignature = Get-AuthenticodeSignature -LiteralPath $nrFile.FullName
$knownCommunityBuild = ($nrHash -eq '91ea4143d9ed1cb90b11a2851cfc68dabe7d1e7414f8dfaa8016d86b99e40be7')
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
    prior_probe_observation = $(if ($knownCommunityBuild) { '2026-09-25, RTX 4080 SUPER: FP16 and RGBA8 evaluation produced nontrivial output; 30-frame D3D11-to-D3D12 bridge passed' } else { $null })
    purpose = 'Read-only provenance diagnostic; signature presence is not authenticity of modified bytes'
} | ConvertTo-Json -Depth 4
