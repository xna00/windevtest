@echo off
setlocal

echo ========================================
echo PrintDriver Installer Build Script
echo ========================================

REM Check for Inno Setup
set ISCC=""
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set ISCC="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
) else if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set ISCC="C:\Program Files\Inno Setup 6\ISCC.exe"
) else (
    echo Error: Inno Setup 6 not found
    echo Please download from https://jrsoftware.org/isdownload.php
    pause
    exit /b 1
)

REM Build program
echo.
echo [1/4] Building program...
call build.bat
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

REM Check if certificate exists
if not exist "cert\PrintDriver.pfx" (
    echo.
    echo [2/4] Creating self-signed certificate...
    echo This requires administrator privileges.
    echo Please click Yes on the UAC prompt.
    powershell -ExecutionPolicy Bypass -Command "Start-Process powershell -ArgumentList '-ExecutionPolicy Bypass -File scripts\create-cert.ps1' -Verb RunAs -Wait"
    if not exist "cert\PrintDriver.pfx" (
        echo Certificate creation failed!
        pause
        exit /b 1
    )
) else (
    echo.
    echo [2/4] Using existing certificate...
)

REM Sign exe
echo.
echo [3/4] Signing program...
powershell -ExecutionPolicy Bypass -File scripts\sign-exe.ps1
if %ERRORLEVEL% neq 0 (
    echo Signing failed!
    pause
    exit /b 1
)

REM Create output directory
if not exist "output" mkdir output

REM Build installer
echo.
echo [4/4] Building installer...
%ISCC% installer\PrintDriver.iss
if %ERRORLEVEL% neq 0 (
    echo Installer build failed!
    pause
    exit /b 1
)

REM Sign installer
echo.
echo Signing installer...
powershell -ExecutionPolicy Bypass -File scripts\sign-exe.ps1 -ExePath ".\output\PrintDriver-Setup.exe"

echo.
echo ========================================
echo Build complete!
echo Installer location: output\PrintDriver-Setup.exe
echo ========================================
pause
