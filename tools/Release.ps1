# ── VIBLI One-Click Release ───────────────────────────────────────────────────
# Chay tu thu muc goc du an:
#   powershell -ExecutionPolicy Bypass -File tools\Release.ps1
#
# Tuy chon:
#   -SkipBuild      Bo qua buoc build (chi chay deploy + installer)
#   -SkipInstaller  Bo qua buoc tao installer
#   -Version "x.y.z" Override version (mac dinh doc tu CMakeLists.txt)
# ─────────────────────────────────────────────────────────────────────────────
param(
    [switch]$SkipBuild,
    [switch]$SkipInstaller,
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$StartTime = Get-Date

# ── Cau hinh ─────────────────────────────────────────────────────────────────
$QtRoot     = "D:\data\qt"
$QtVersion  = "6.11.0"
$QtCompiler = "mingw_64"
$MinGW      = "mingw1310_64"

# Tim Inno Setup compiler
$InnoPaths = @(
    "D:\data\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 5\ISCC.exe",
    "C:\Program Files\Inno Setup 5\ISCC.exe"
)
$ISCC = $InnoPaths | Where-Object { Test-Path $_ } | Select-Object -First 1

# ── Helpers ───────────────────────────────────────────────────────────────────
function Step($n, $total, $msg) {
    Write-Host ""
    Write-Host "  [$n/$total] $msg" -ForegroundColor Yellow
}

function OK($msg) {
    Write-Host "        OK  $msg" -ForegroundColor Green
}

function Fail($msg) {
    Write-Host ""
    Write-Host "  FAILED: $msg" -ForegroundColor Red
    exit 1
}

# ── Doc version tu CMakeLists.txt ─────────────────────────────────────────────
if (-not $Version) {
    $cmakeContent = Get-Content "CMakeLists.txt" -Raw
    if ($cmakeContent -match 'project\(VIBLI VERSION (\d+\.\d+\.\d+)') {
        $Version = $Matches[1]
    } else {
        $Version = "1.0.0"
    }
}

$TotalSteps = 4
if ($SkipBuild)     { $TotalSteps -= 2 }
if ($SkipInstaller) { $TotalSteps -= 1 }

# ── Header ────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "  VIBLI Release  v$Version" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan

# ── Setup PATH ────────────────────────────────────────────────────────────────
$env:PATH = "$QtRoot\Tools\$MinGW\bin;$QtRoot\Tools\Ninja;$QtRoot\Tools\CMake_64\bin;$QtRoot\$QtVersion\$QtCompiler\bin;$env:PATH"

$step = 0

# ── Buoc 1+2: Build ───────────────────────────────────────────────────────────
if (-not $SkipBuild) {
    $step++
    Step $step $TotalSteps "Configure (Release)"
    cmake --preset release 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Fail "cmake configure" }
    OK "Configure done"

    $step++
    Step $step $TotalSteps "Build"
    $buildOutput = cmake --build build --parallel 2>&1
    if ($LASTEXITCODE -ne 0) {
        $buildOutput | Select-String "error:" | ForEach-Object { Write-Host $_ -ForegroundColor Red }
        Fail "cmake build"
    }
    OK "Build done  ->  build\VIBLI.exe"
}

# ── Buoc 3: Deploy ────────────────────────────────────────────────────────────
$step++
Step $step $TotalSteps "Deploy Qt dependencies"

if (Test-Path "deploy") { Remove-Item -Recurse -Force "deploy" }
New-Item -ItemType Directory -Path "deploy" | Out-Null

if (-not (Test-Path "build\VIBLI.exe")) {
    Fail "build\VIBLI.exe khong ton tai. Chay lai khong co -SkipBuild."
}
Copy-Item "build\VIBLI.exe" "deploy\"

$deployResult = & { $ErrorActionPreference = "Continue"; windeployqt --release --no-translations --no-system-d3d-compiler "deploy\VIBLI.exe" 2>&1 }
# Kiem tra ket qua bang file thay vi exit code (windeployqt co the tra warning qua stderr)
if (-not (Test-Path "deploy\Qt6Core.dll")) {
    $deployResult | Write-Host -ForegroundColor Red
    Fail "windeployqt: Qt6Core.dll khong duoc copy"
}
OK "Qt DLLs deployed"

Copy-Item "resources\fonts\MaterialSymbolsRounded.ttf" "deploy\" -Force
OK "Font copied"

if (Test-Path "yt_dlp") {
    Copy-Item -Recurse "yt_dlp" "deploy\yt_dlp" -Force
    OK "yt_dlp copied"
} else {
    Write-Host "        WARN: yt_dlp folder not found" -ForegroundColor DarkYellow
}

# ── Buoc 4: Installer ─────────────────────────────────────────────────────────
if (-not $SkipInstaller) {
    $step++
    Step $step $TotalSteps "Build installer (Inno Setup)"

    if (-not $ISCC) {
        Write-Host ""
        Write-Host "  WARN: Inno Setup khong tim thay." -ForegroundColor DarkYellow
        Write-Host "  Tai tai: https://jrsoftware.org/isdl.php" -ForegroundColor DarkYellow
        Write-Host "  Sau khi cai, chay lai script nay." -ForegroundColor DarkYellow
        Write-Host ""
        Write-Host "  Hoac chay thu cong:" -ForegroundColor Gray
        Write-Host "    ISCC.exe installer\vibli_setup.iss" -ForegroundColor Gray
    } else {
        if (-not (Test-Path "dist")) { New-Item -ItemType Directory -Path "dist" | Out-Null }

        $isccProc = Start-Process -FilePath $ISCC -ArgumentList "/Q", "installer\vibli_setup.iss" -Wait -PassThru -NoNewWindow
        if ($isccProc.ExitCode -ne 0) { Fail "Inno Setup compiler (exit $($isccProc.ExitCode))" }
        if ($LASTEXITCODE -ne 0) { Fail "Inno Setup compiler" }

        $installer = "dist\VIBLI_Setup_$Version.exe"
        if (Test-Path $installer) {
            $sizeMB = [math]::Round((Get-Item $installer).Length / 1MB, 1)
            OK "Installer: $installer  ($sizeMB MB)"
        } else {
            OK "Installer built  ->  dist\"
        }
    }
}

# ── Tong ket ──────────────────────────────────────────────────────────────────
$elapsed = [math]::Round(((Get-Date) - $StartTime).TotalSeconds, 1)
Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "  Done in ${elapsed}s" -ForegroundColor Green
Write-Host "  deploy\VIBLI.exe          <- portable" -ForegroundColor Green
if (-not $SkipInstaller -and $ISCC) {
    Write-Host "  dist\VIBLI_Setup_$Version.exe  <- installer" -ForegroundColor Green
}
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
