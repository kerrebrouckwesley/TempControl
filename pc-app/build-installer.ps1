# Bouwt dist\TempMonitor-Setup-<versie>.exe
#   powershell -ExecutionPolicy Bypass -File .\build-installer.ps1
#   powershell -ExecutionPolicy Bypass -File .\build-installer.ps1 -OutputDir E:\   (bv. rechtstreeks naar een USB-stick)
# Vereist: .NET 10 SDK en Inno Setup 6 (winget install JRSoftware.InnoSetup)

param([string]$OutputDir)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $OutputDir) { $OutputDir = Join-Path (Split-Path $root) 'dist' }
$installerDir = Join-Path $root 'installer'
$publishDir = Join-Path $installerDir 'publish'
$presentMonVersion = '2.6.0'
$presentMonCache = Join-Path $installerDir "PresentMon-$presentMonVersion-x64.exe"

$version = ([xml](Get-Content (Join-Path $root 'TempMonitor.csproj'))).Project.PropertyGroup.Version | Where-Object { $_ } | Select-Object -First 1

# 1. App publiceren (self-contained: .NET hoeft niet op de doel-pc te staan; WinExe: geen consolevenster)
if (Test-Path $publishDir) { Remove-Item $publishDir -Recurse -Force }
dotnet publish (Join-Path $root 'TempMonitor.csproj') -c Release -r win-x64 --self-contained true `
    -p:OutputType=WinExe -p:DebugType=none -o $publishDir
if ($LASTEXITCODE) { throw 'dotnet publish mislukt' }

# 2. PresentMon (FPS-meting) ophalen
if (-not (Test-Path $presentMonCache)) {
    $url = "https://github.com/GameTechDev/PresentMon/releases/download/v$presentMonVersion/PresentMon-$presentMonVersion-x64.exe"
    Write-Host "PresentMon downloaden: $url"
    Invoke-WebRequest $url -OutFile $presentMonCache
}
Copy-Item $presentMonCache (Join-Path $publishDir 'PresentMon.exe')

# 3. Installer compileren
$iscc = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup 6 niet gevonden (winget install JRSoftware.InnoSetup)' }

& $iscc "/DAppVersion=$version" "/O$OutputDir" (Join-Path $installerDir 'TempMonitor.iss')
if ($LASTEXITCODE) { throw 'Inno Setup mislukt' }

Write-Host "`nKlaar: $(Join-Path $OutputDir "TempMonitor-Setup-$version.exe")"
