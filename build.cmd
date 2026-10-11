@echo off
rem Usage: build.cmd [fre|chk]     (default: fre)
rem If the WDK 7.1 is not in C:\WinDDK\7600.16385.1, set WDK71 to its folder first.

setlocal
if not defined WDK71 set WDK71=C:\WinDDK\7600.16385.1
set CFG=%1
if "%CFG%"=="" set CFG=fre
if /i not "%CFG%"=="fre" if /i not "%CFG%"=="chk" (
    echo Usage: build.cmd [fre^|chk]
    endlocal
    exit /b 1
)

if not exist "%WDK71%\bin\setenv.bat" (
    echo Windows Driver Kit 7.1.0 not found in "%WDK71%".
    echo Install it, or set WDK71 to the folder it was installed to.
    endlocal
    exit /b 1
)

for %%I in ("%~dp0.") do set SRCDIR=%%~sI
if not "%SRCDIR%"=="%SRCDIR: =%" (
    echo The source folder path contains spaces and this drive has no short
    echo 8.3 names for it. Move the source to a path without spaces.
    endlocal
    exit /b 1
)

set DDKBUILDENV=
call "%WDK71%\bin\setenv.bat" %WDK71%\ %CFG% x64 WLH no_oacr
cd /d %SRCDIR%

if exist build%CFG%_wlh_amd64.err del build%CFG%_wlh_amd64.err
"%WDK71%\bin\x86\amd64\build.exe" -cZg
if exist build%CFG%_wlh_amd64.err (
    echo.
    echo Build failed, see build%CFG%_wlh_amd64.err
    endlocal
    exit /b 1
)
if not exist obj%CFG%_wlh_amd64\amd64\e1000ev.sys (
    echo.
    echo Build failed, see build%CFG%_wlh_amd64.log
    endlocal
    exit /b 1
)

call "%~dp0sign.cmd" %CFG%
endlocal
