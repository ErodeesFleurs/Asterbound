param(
    [string]$Compiler = "$env:ProgramFiles\Inno Setup 7\ISCC.exe",
    [string]$OutputDirectory = "installer"
)

$ErrorActionPreference = "Stop"
$repository = (Resolve-Path "$PSScriptRoot/../../..").Path
$version = (Get-Content -Raw "$repository/version.json" | ConvertFrom-Json).version
& $Compiler "/DAppVersion=$version" "/O$OutputDirectory" "$repository/scripts/inno/setup.iss"
exit $LASTEXITCODE
