$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$instances = if (Test-Path -LiteralPath $vswhere) { & $vswhere -all -products '*' -format json | ConvertFrom-Json } else { @() }
$commands = foreach ($name in 'git','python','pwsh','cmake','ninja','cl') {
    $command = Get-Command $name -ErrorAction SilentlyContinue
    [pscustomobject]@{ name=$name; path=$command.Source; status=$(if ($command) {'FOUND'} else {'NOT_ON_PATH'}) }
}
[ordered]@{
    time_utc = [DateTime]::UtcNow.ToString('o')
    visual_studio = @($instances | Select-Object installationPath,installationVersion)
    commands = @($commands)
    sdk_versions = @(Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/Lib" -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
    reference_root = $env:RAZKOLBAS_REFERENCE_ROOT
    os_display_adapters = @(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    adapter_note = 'OS inventory only; actual render adapter must come from the Skyrim D3D11 device.'
} | ConvertTo-Json -Depth 5
