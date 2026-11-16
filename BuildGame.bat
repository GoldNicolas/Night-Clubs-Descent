@echo off
rem Build, cook and package the game into Builds\ and check that it contains everything the editor shows.
rem   BuildGame.bat                         Development build + all checks
rem   BuildGame.bat -Config Shipping -Zip   final build, zipped
rem Options and details: Tools\Build\BuildGame.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Build\BuildGame.ps1" %*
set RESULT=%ERRORLEVEL%
if "%~1"=="" pause
exit /b %RESULT%
