# VIBLI Development Menu
# Usage: .\show.ps1

function Show-Menu {
    Clear-Host
    Write-Host "================================" -ForegroundColor Cyan
    Write-Host "   VIBLI Development Menu" -ForegroundColor Cyan
    Write-Host "================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  [1] Setup Environment" -ForegroundColor Yellow
    Write-Host "  [2] Configure Project (cmake --preset default)" -ForegroundColor Green
    Write-Host "  [3] Build Debug (parallel)" -ForegroundColor Green
    Write-Host "  [4] Run Debug Build" -ForegroundColor Green
    Write-Host "  [5] Clean Build Directory" -ForegroundColor Red
    Write-Host ""
    Write-Host "  [6] Build Release (full package)" -ForegroundColor Magenta
    Write-Host "  [7] Build Release (skip build)" -ForegroundColor Magenta
    Write-Host "  [8] Build Release (skip installer)" -ForegroundColor Magenta
    Write-Host ""
    Write-Host "  [9] Configure + Build + Run (Quick Start)" -ForegroundColor Cyan
    Write-Host "  [0] Rebuild from Scratch" -ForegroundColor Red
    Write-Host ""
    Write-Host "  [Q] Quit" -ForegroundColor Gray
    Write-Host ""
}

function Setup-Environment {
    Write-Host "`n[Setup Environment]" -ForegroundColor Cyan
    $env:PATH = "D:\data\qt\Tools\mingw1310_64\bin;D:\data\qt\Tools\Ninja;D:\data\qt\Tools\CMake_64\bin;$env:PATH"
    Write-Host "Added MinGW, Ninja, CMake to PATH" -ForegroundColor Green
    Write-Host "Environment ready for build" -ForegroundColor Green
    Pause
}

function Configure-Project {
    Write-Host "`n[Configure Project]" -ForegroundColor Cyan
    Setup-Environment
    cmake --preset default
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Configuration successful" -ForegroundColor Green
    } else {
        Write-Host "Configuration failed" -ForegroundColor Red
    }
    Pause
}

function Build-Debug {
    Write-Host "`n[Build Debug]" -ForegroundColor Cyan
    Setup-Environment
    cmake --build build --parallel
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build successful" -ForegroundColor Green
    } else {
        Write-Host "Build failed" -ForegroundColor Red
    }
    Pause
}

function Run-Debug {
    Write-Host "`n[Run Debug Build]" -ForegroundColor Cyan
    if (-not (Test-Path "build\VIBLI.exe")) {
        Write-Host "VIBLI.exe not found. Build first!" -ForegroundColor Red
        Pause
        return
    }
    $env:PATH = "D:\data\qt\6.11.0\mingw_64\bin;D:\data\qt\Tools\mingw1310_64\bin;$env:PATH"
    Write-Host "Starting VIBLI..." -ForegroundColor Green
    & ".\build\VIBLI.exe"
}

function Clean-Build {
    Write-Host "`n[Clean Build Directory]" -ForegroundColor Red
    $confirm = Read-Host "Delete build/, deploy/, dist/ folders? (y/N)"
    if ($confirm -eq 'y' -or $confirm -eq 'Y') {
        if (Test-Path "build") {
            Remove-Item -Recurse -Force "build"
            Write-Host "Removed build/" -ForegroundColor Green
        }
        if (Test-Path "deploy") {
            Remove-Item -Recurse -Force "deploy"
            Write-Host "Removed deploy/" -ForegroundColor Green
        }
        if (Test-Path "dist") {
            Remove-Item -Recurse -Force "dist"
            Write-Host "Removed dist/" -ForegroundColor Green
        }
        Write-Host "Clean complete" -ForegroundColor Green
    } else {
        Write-Host "Cancelled" -ForegroundColor Yellow
    }
    Pause
}

function Build-Release-Full {
    Write-Host "`n[Build Release - Full Package]" -ForegroundColor Magenta
    powershell -ExecutionPolicy Bypass -File tools\Release.ps1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Release build complete" -ForegroundColor Green
        Write-Host "  -> deploy\VIBLI.exe (portable)" -ForegroundColor Cyan
        Write-Host "  -> dist\VIBLI_Setup_*.exe (installer)" -ForegroundColor Cyan
    } else {
        Write-Host "Release build failed" -ForegroundColor Red
    }
    Pause
}

function Build-Release-SkipBuild {
    Write-Host "`n[Build Release - Skip Build]" -ForegroundColor Magenta
    powershell -ExecutionPolicy Bypass -File tools\Release.ps1 -SkipBuild
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Release package complete" -ForegroundColor Green
    } else {
        Write-Host "Release package failed" -ForegroundColor Red
    }
    Pause
}

function Build-Release-SkipInstaller {
    Write-Host "`n[Build Release - Skip Installer]" -ForegroundColor Magenta
    powershell -ExecutionPolicy Bypass -File tools\Release.ps1 -SkipInstaller
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Release build complete (no installer)" -ForegroundColor Green
    } else {
        Write-Host "Release build failed" -ForegroundColor Red
    }
    Pause
}

function Quick-Start {
    Write-Host "`n[Quick Start - Configure + Build + Run]" -ForegroundColor Cyan
    Setup-Environment
    
    Write-Host "`nStep 1/3: Configuring..." -ForegroundColor Yellow
    cmake --preset default
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Configuration failed" -ForegroundColor Red
        Pause
        return
    }
    
    Write-Host "`nStep 2/3: Building..." -ForegroundColor Yellow
    cmake --build build --parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed" -ForegroundColor Red
        Pause
        return
    }
    
    Write-Host "`nStep 3/3: Running..." -ForegroundColor Yellow
    $env:PATH = "D:\data\qt\6.11.0\mingw_64\bin;D:\data\qt\Tools\mingw1310_64\bin;$env:PATH"
    & ".\build\VIBLI.exe"
}

function Rebuild-FromScratch {
    Write-Host "`n[Rebuild from Scratch]" -ForegroundColor Red
    $confirm = Read-Host "Clean + Configure + Build? (y/N)"
    if ($confirm -ne 'y' -and $confirm -ne 'Y') {
        Write-Host "Cancelled" -ForegroundColor Yellow
        Pause
        return
    }
    
    Setup-Environment
    
    Write-Host "`nStep 1/3: Cleaning..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
        Write-Host "Removed build/" -ForegroundColor Green
    }
    
    Write-Host "`nStep 2/3: Configuring..." -ForegroundColor Yellow
    cmake --preset default
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Configuration failed" -ForegroundColor Red
        Pause
        return
    }
    
    Write-Host "`nStep 3/3: Building..." -ForegroundColor Yellow
    cmake --build build --parallel
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Rebuild complete" -ForegroundColor Green
    } else {
        Write-Host "Build failed" -ForegroundColor Red
    }
    Pause
}

# Main loop
do {
    Show-Menu
    $choice = Read-Host "Select option"
    
    switch ($choice) {
        '1' { Setup-Environment }
        '2' { Configure-Project }
        '3' { Build-Debug }
        '4' { Run-Debug }
        '5' { Clean-Build }
        '6' { Build-Release-Full }
        '7' { Build-Release-SkipBuild }
        '8' { Build-Release-SkipInstaller }
        '9' { Quick-Start }
        '0' { Rebuild-FromScratch }
        'Q' { 
            Write-Host "`nGoodbye!" -ForegroundColor Cyan
            exit 
        }
        'q' { 
            Write-Host "`nGoodbye!" -ForegroundColor Cyan
            exit 
        }
        default {
            Write-Host "`nInvalid option. Press any key to continue..." -ForegroundColor Red
            Pause
        }
    }
} while ($true)
