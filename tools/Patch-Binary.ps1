param(
    [Parameter(Mandatory)][string]$Manifest,
    [Parameter(Mandatory)][string]$Input,
    [string]$Output,
    [switch]$WhatIf,
    [switch]$Restore
)
$ErrorActionPreference = 'Stop'
if (-not $Output) { $Output = $Input + $(if ($Restore) { '.restored' } else { '.patched' }) }
$patchArgs = @("$PSScriptRoot/file_patch.py", '--manifest', $Manifest, '--input', $Input, '--output', $Output)
if ($WhatIf) { $patchArgs += '--what-if' }
if ($Restore) { $patchArgs += '--restore' }
& python @patchArgs
exit $LASTEXITCODE
