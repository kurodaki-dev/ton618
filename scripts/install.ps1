# TON618 installer - Windows
# Usage: powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"
#
# Always installs a precompiled binary from a GitHub Release (built by
# .github/workflows/release.yml). Nothing is ever compiled on this machine.
#
# Set $env:TON618_VERSION to install a specific release tag instead of the
# latest one, e.g.: $env:TON618_VERSION="beta-1.0.0"; irm .../install.ps1 | iex

$ErrorActionPreference = "Stop"

$Repo    = "kurodaki-dev/ton618"
$Version = if ($env:TON618_VERSION) { $env:TON618_VERSION } else { "latest" }

$InstallDir = Join-Path $env:LOCALAPPDATA "ton618"
$Target     = Join-Path $InstallDir "ton618.exe"

if ($Version -eq "latest") {
    $DownloadUrl = "https://github.com/$Repo/releases/latest/download/ton618-windows-x64.exe"
} else {
    $DownloadUrl = "https://github.com/$Repo/releases/download/$Version/ton618-windows-x64.exe"
}

Write-Host "[ton618] Installing to $InstallDir..." -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

Write-Host "[ton618] Downloading $Version from GitHub Releases..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $DownloadUrl -OutFile $Target

# Add the install dir to the user's PATH if it isn't already there.
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -notlike "*$InstallDir*") {
    $newPath = if ([string]::IsNullOrEmpty($userPath)) { $InstallDir } else { "$userPath;$InstallDir" }
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    $env:Path = "$env:Path;$InstallDir"
    Write-Host "[ton618] Added to the user PATH. Open a new terminal so 'ton618' is picked up." -ForegroundColor Yellow
}

Write-Host "[ton618] Install complete. Try: ton618 --help" -ForegroundColor Green
