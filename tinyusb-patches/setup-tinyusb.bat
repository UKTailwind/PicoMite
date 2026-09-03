@echo off
rem setup-tinyusb.bat - create the patched TinyUSB 0.21 tree PicoMite builds
rem against, by running setup-tinyusb.sh under Git Bash (Git for Windows).
setlocal

rem Prefer a bash already on PATH...
where bash >nul 2>&1
if %errorlevel%==0 (
    bash "%~dp0setup-tinyusb.sh"
    exit /b %errorlevel%
)

rem ...otherwise try the default Git for Windows locations.
if exist "%ProgramFiles%\Git\bin\bash.exe" (
    "%ProgramFiles%\Git\bin\bash.exe" "%~dp0setup-tinyusb.sh"
    exit /b %errorlevel%
)
if exist "%ProgramW6432%\Git\bin\bash.exe" (
    "%ProgramW6432%\Git\bin\bash.exe" "%~dp0setup-tinyusb.sh"
    exit /b %errorlevel%
)

echo Could not find 'bash'. Install Git for Windows, then either re-run this
echo file or run tinyusb-patches\setup-tinyusb.sh from a Git Bash shell.
exit /b 1
