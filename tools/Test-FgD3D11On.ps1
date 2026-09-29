param(
    [Parameter(Mandatory = $true)][string]$SdkBin
)

$probe = Join-Path $PSScriptRoot '..\build\win-release\bin\Release\RazKolbasFgStreamlineProbe.exe'
if (-not (Test-Path -LiteralPath $probe)) { throw "FG probe executable is missing: $probe" }
$resolvedSdkBin = (Resolve-Path -LiteralPath $SdkBin -ErrorAction Stop).Path
$output = & $probe $resolvedSdkBin --facade-on 2>&1
$status = $LASTEXITCODE
$text = $output -join "`n"
if ($status -ne 0 -or $text -notmatch 'FG-D3D11 facade=1' -or
    $text -notmatch 'FG-On generatedObserved=1' -or
    ([regex]::Matches($text, 'actualPresented=2')).Count -ne 8 -or
    ([regex]::Matches($text, 'FG-On Present frame=\d+ hr=0x0')).Count -ne 8 -or
    $text -notmatch 'FG-On drain Present=0x0' -or
    $text -notmatch 'slShutdown=0') {
    throw "D3D11-to-Streamline FG probe failed (exit $status):`n$text"
}
$text
