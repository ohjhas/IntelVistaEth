@echo off
rem test sign the driver
rem run AFTER build.cmd (build.cmd already calls it).  Usage: sign.cmd [fre|chk]   (default: fre)

setlocal
if not defined WDK71 set WDK71=C:\WinDDK\7600.16385.1
set CFG=%1
if "%CFG%"=="" set CFG=fre
set PATH=%WDK71%\bin\x86;%WDK71%\bin\selfsign;%PATH%
set CERTNAME=e1000ev Test

cd /d "%~dp0"
if not exist package mkdir package
copy /y obj%CFG%_wlh_amd64\amd64\e1000ev.sys package\ >nul || goto :fail
copy /y e1000ev.inf package\ >nul || goto :fail
if exist package\e1000ev.cat del package\e1000ev.cat
if exist package\e1000ev_test.cer del package\e1000ev_test.cer

certmgr -put -c -n "%CERTNAME%" -s -r currentUser PrivateCertStore package\e1000ev_test.cer >nul 2>&1
if not exist package\e1000ev_test.cer (
    echo Creating test certificate "%CERTNAME%" in your certificate store...
    makecert -r -pe -a sha1 -ss PrivateCertStore -n "CN=%CERTNAME%" package\e1000ev_test.cer >nul || goto :fail
)

if not exist e1000ev_test.cer copy /y package\e1000ev_test.cer e1000ev_test.cer >nul
fc /b e1000ev_test.cer package\e1000ev_test.cer >nul 2>&1 || (
    echo.
    echo NOTE: this package is signed with your own test certificate, not the
    echo       e1000ev_test.cer in the source folder. On Vista, import
    echo       package\e1000ev_test.cer instead.
    echo.
)

signtool sign /s PrivateCertStore /n "%CERTNAME%" package\e1000ev.sys >nul || goto :fail
Inf2Cat /driver:package /os:Vista_X64 >nul || goto :fail
signtool sign /s PrivateCertStore /n "%CERTNAME%" package\e1000ev.cat >nul || goto :fail

echo.
echo Signed package is in %~dp0package
endlocal
exit /b 0

:fail
echo Signing failed.
endlocal
exit /b 1
