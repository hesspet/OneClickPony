@echo off
setlocal
set "PROJECT_DIRECTORY=%~dp0..\.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0FirmwareReleaseVorbereiten.ps1" %*
exit /b %ERRORLEVEL%
