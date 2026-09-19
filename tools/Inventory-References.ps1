param([string]$ReferenceRoot = $env:RAZKOLBAS_REFERENCE_ROOT, [switch]$Verify)
$ErrorActionPreference = 'Stop'
if (-not $ReferenceRoot) { throw 'Supply -ReferenceRoot or RAZKOLBAS_REFERENCE_ROOT' }
$inventoryArgs = @("$PSScriptRoot/reference_inventory.py", '--root', $ReferenceRoot)
if ($Verify) { $inventoryArgs += '--verify' }
& python @inventoryArgs
exit $LASTEXITCODE
