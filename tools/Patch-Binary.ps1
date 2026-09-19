param(
    [Parameter(Mandatory)][string]$Manifest,
    [Parameter(Mandatory)][Alias('Input')][string]$SourceFile,
    [string]$Output,
    [switch]$WhatIf,
    [switch]$Restore
)
$ErrorActionPreference = 'Stop'
if (-not $Output) { $Output = $SourceFile + $(if ($Restore) { '.restored' } else { '.patched' }) }
$patchArgs = @("$PSScriptRoot/file_patch.py", '--manifest', $Manifest, '--input', $SourceFile, '--output', $Output)
if ($WhatIf) { $patchArgs += '--what-if' }
if ($Restore) { $patchArgs += '--restore' }
& python @patchArgs
exit $LASTEXITCODE
