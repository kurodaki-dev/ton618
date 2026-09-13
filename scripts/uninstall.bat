@echo off
powershell -NoProfile -ExecutionPolicy Bypass -Command "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/uninstall.ps1 | iex"
pause
