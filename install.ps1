# SPDX-License-Identifier: MIT
# Copyright (c) 2026 SpaceNavigator for Illustrator contributors

param([string]$Destination = (Join-Path $env:ProgramFiles 'Adobe\Adobe Illustrator 2026\Plug-ins'))
$ErrorActionPreference = 'Stop'

if (Get-Process Illustrator -ErrorAction SilentlyContinue) { throw 'Close Illustrator yourself before installing the update.' }
foreach ($name in @('IllustratorNavigator.aip', 'NavigatorInputBridge.exe')) {
    $Source = Join-Path $PSScriptRoot ('out\' + $name)
    if (!(Test-Path -LiteralPath $Source)) { throw "Missing build output: $name" }
}
foreach ($name in @('NavigatorInputBridge.exe', 'IllustratorNavigator.aip')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('out\' + $name)) -Destination (Join-Path $Destination $name)
}
Write-Host 'Installed. Open Illustrator yourself, then select Window > SpaceNavigator > Enable navigation.'



