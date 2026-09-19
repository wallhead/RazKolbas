$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '../..')
try {
    & python -m unittest discover -s tests/integration -p test_inventory.py -v
    if ($LASTEXITCODE -ne 0) { throw 'Inventory regression tests failed' }
} finally { Pop-Location }
