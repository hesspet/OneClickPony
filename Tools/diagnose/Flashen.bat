@echo off
setlocal
call "%~dp0..\env.bat"
chcp 65001 >nul
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
set "PROJECT_DIRECTORY=%~dp0..\.."
set "PLATFORMIO=%APPDATA%\Python\Python313\Scripts\platformio.exe"

if not exist "%PLATFORMIO%" set "PLATFORMIO=platformio"
"%PLATFORMIO%" run --project-dir "%PROJECT_DIRECTORY%" --environment diagnose-c3 --target upload --upload-port %DIAGNOSE_COM_PORT%
exit /b %ERRORLEVEL%
