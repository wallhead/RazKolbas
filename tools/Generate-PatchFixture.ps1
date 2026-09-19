$ErrorActionPreference = 'Stop'
$fixturePath = Join-Path $PSScriptRoot '../tests/fixtures/patch-input.bin'
$expected = '2db31f4e09597946e56e859813bfdad7046d457c32332c3a882654d1e3ffdf67'
if (Test-Path -LiteralPath $fixturePath) {
    if ((Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Existing fixture changed; refusing overwrite' }
} else {
    [IO.File]::WriteAllBytes([IO.Path]::GetFullPath($fixturePath), [byte[]](0xb8,1,0,0,0,0xc3))
}
Write-Output ([IO.Path]::GetFullPath($fixturePath))
