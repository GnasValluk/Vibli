# ── VIBLI Release Build Script ────────────────────────────────────────────────
# Chạy script này từ thư mục gốc dự án:
#   powershell -ExecutionPolicy Bypass -File tools\RunBuildRelease.ps1

$ErrorActionPreference = "Stop"

$QtRoot    = "D:\data\qt"
$QtVersion = "6.11.0"
$QtCompiler= "mingw_64"
$MinGW     = "mingw1310_64"

$env:PATH = "$QtRoot\Tools\$MinGW\bin;$QtRoot\Tools\Ninja;$QtRoot\Tools\CMake_64\bin;$QtRoot\$QtVersion\$QtCompiler\bin;$env:PATH"

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "  VIBLI Release Build" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan

# ── Bước 1: CMake configure release ──────────────────────────────────────────
Write-Host ""
Write-Host "[1/3] Configuring (Release)..." -ForegroundColor Yellow
cmake --preset release
if ($LASTEXITCODE -ne 0) { Write-Host "Configure failed." -ForegroundColor Red; exit 1 }

# ── Bước 2: Build ─────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "[2/3] Building..." -ForegroundColor Yellow
cmake --build build --parallel
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed." -ForegroundColor Red; exit 1 }

# ── Bước 3: Deploy Qt DLLs ────────────────────────────────────────────────────
Write-Host ""
Write-Host "[3/3] Deploying Qt dependencies..." -ForegroundColor Yellow

if (Test-Path "deploy") { Remove-Item -Recurse -Force "deploy" }
New-Item -ItemType Directory -Path "deploy" | Out-Null

Copy-Item "build\VIBLI.exe" "deploy\"

windeployqt --release --no-translations --no-system-d3d-compiler "deploy\VIBLI.exe"
if ($LASTEXITCODE -ne 0) { Write-Host "windeployqt failed." -ForegroundColor Red; exit 1 }

# Copy font file
Copy-Item "$QtRoot\$QtVersion\$QtCompiler\bin\MaterialSymbolsRounded.ttf" "deploy\" -ErrorAction SilentlyContinue
Copy-Item "resources\fonts\MaterialSymbolsRounded.ttf" "deploy\" -Force

# ── Xong ──────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "  Build complete!" -ForegroundColor Green
Write-Host "  Output: deploy\" -ForegroundColor Green
Write-Host ""
Write-Host "  Next: Open installer\vibli_setup.iss" -ForegroundColor Green
Write-Host "        in Inno Setup and press Ctrl+F9" -ForegroundColor Green
Write-Host "        to create dist\VIBLI_Setup_1.0.0.exe" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
