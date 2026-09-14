# TON618 uninstaller - Windows
# Usage: powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/uninstall.ps1 | iex"
# (or simply: ton618 --uninstall-ton)

$InstallDir = Join-Path $env:LOCALAPPDATA "ton618"

if (Test-Path $InstallDir) {
    Remove-Item -Recurse -Force $InstallDir
    Write-Host "[ton618] Removed: $InstallDir" -ForegroundColor Green
} else {
    Write-Host "[ton618] No install found." -ForegroundColor Yellow
}

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -like "*$InstallDir*") {
    $parts = $userPath -split ";" | Where-Object { $_ -ne "" -and $_ -ne $InstallDir }
    [Environment]::SetEnvironmentVariable("Path", ($parts -join ";"), "User")
    Write-Host "[ton618] Removed from the user PATH." -ForegroundColor Green
}

Write-Host "[ton618] Uninstall complete." -ForegroundColor Green
