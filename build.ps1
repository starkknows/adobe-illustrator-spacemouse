# SPDX-License-Identifier: MIT
# Copyright (c) 2026 SpaceNavigator for Illustrator contributors

param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$Sdk = Join-Path $PSScriptRoot 'sdk\Adobe Illustrator 2026 SDK'
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$MsBuild = & $VsWhere -latest -products * -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$MsBuild) { throw 'Visual Studio MSBuild not found.' }
if (!(Get-Command $Python -ErrorAction SilentlyContinue)) { throw 'Python not found. Install Python 3 or pass -Python with its executable path.' }
if (!(Test-Path -LiteralPath (Join-Path $Sdk 'illustratorapi\illustrator\AIDocumentView.h'))) { throw 'Adobe Illustrator SDK not found. See README.md for download and folder setup.' }
Push-Location $PSScriptRoot
try {
    # Keep JSON inside Python: Windows PowerShell strips embedded quotes in native arguments.
    & $Python (Join-Path $PSScriptRoot 'generate_pipl.py')
    if ($LASTEXITCODE) { throw 'PiPL generation failed.' }
    & $MsBuild (Join-Path $PSScriptRoot 'NavigatorInputBridge.vcxproj') /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE) { throw 'Input bridge build failed.' }
    & $MsBuild (Join-Path $PSScriptRoot 'InputTests.vcxproj') /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE) { throw 'Input checks build failed.' }
    & (Join-Path $PSScriptRoot 'out\InputTests.exe')
    if ($LASTEXITCODE) { throw 'Input checks failed.' }
    & $MsBuild (Join-Path $PSScriptRoot 'IllustratorNavigator.vcxproj') /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE) { throw 'Plugin build failed.' }
} finally { Pop-Location }





