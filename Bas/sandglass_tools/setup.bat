@echo off
rem  setup.bat - Prince of Pico, everything in one go, on Windows.
rem
rem    setup.bat                      fetch the source release and convert it
rem    setup.bat "C:\path\to\copy"    use a copy you already have
rem
rem  Leaves a directory called board\ holding exactly the files that go on the
rem  board's drive.  What ends up in there is the game's own content in another
rem  format and is not yours to pass on - see README.md.

setlocal
cd /d "%~dp0"

set PY=
for %%P in (py.exe) do if not "%%~$PATH:P"=="" set PY=py -3
if not defined PY for %%P in (python.exe) do if not "%%~$PATH:P"=="" set PY=python

if not defined PY (
    echo.
    echo Python was not found on the path.
    echo.
    echo Install Python 3.8 or later from https://www.python.org/downloads/
    echo and tick "Add python.exe to PATH" while you do it, then run this again.
    echo.
    exit /b 1
)

%PY% setup.py %*
if errorlevel 1 (
    echo.
    echo That did not finish.  The message above says why.
    exit /b 1
)

exit /b 0
