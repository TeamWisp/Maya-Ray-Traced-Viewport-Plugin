@Echo Off

REM ##### COLOR SUPPORT #####
SETLOCAL EnableDelayedExpansion
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do     rem"') do (
  set "DEL=%%a"
)
SET aqua=03
SET light_green=0a
SET red=04
SET title_color=0b
SET header_color=%aqua%
REM ##### COLOR SUPPORT #####

REM ##### LOG NAMES #####
SET log_prefix="cmd_log_"
SET log_suffix=".txt"
REM ##### LOG NAMES #####

REM ##### MAIN #####

call :colorEcho %title_color% "==================================="
call :colorEcho %title_color% "           Wisp Installer          "
call :colorEcho %title_color% "==================================="

call :downloadDeps

call :genVS15Win64 
REM >> test.txt 2>&1

call :colorEcho %light_green% "Installation Finished!"

pause
EXIT
REM ##### MAIN #####

REM ##### DOWNLOAD DEPS #####
:downloadDeps
call :colorEcho %header_color% "#### Downloading Dependencies ####"
git submodule init
git submodule update 
EXIT /B 0
REM ##### DOWNLOAD DEPS #####

REM ##### GEN PROJECTS #####
:genVS15Win64
call :colorEcho %header_color% "#### Generating Visual Studio 15 2017 Win64 Project. ####"
mkdir build_vs2017_win64
cd build_vs2017_win64
cmake -G "Visual Studio 15 2017" -A x64 ..
if errorlevel 1 call :colorecho %red% "CMake finished with errors"
cd ..
EXIT /B 0

REM ##### GEN CMAKE PROJECTS #####

REM ##### COLOR SUPPORT #####
:colorEcho
<nul set /p ".=%DEL%" > "%~2"
findstr /v /a:%1 /R "^$" "%~2" nul
del "%~2" > nul 2>&1i
echo.
EXIT /B 0
REM ##### COLOR SUPPORT #####
