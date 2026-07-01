#Requires -Version 5.1
<#
.SYNOPSIS
    Stage 1 of the build: compile the pinned OpenUSD release (core-only,
    monolithic, Python-free) into thirdparty/usd-install/.

.DESCRIPTION
    Clones OpenUSD at the pinned tag and runs its own build_usd.py, which builds
    USD's third-party dependencies (oneTBB) and USD itself. Everything lands
    under thirdparty/ (gitignored). The cold build takes tens of minutes; rerun
    only when bumping the pin or with -Clean.

    All build_usd.py flags below were verified against the v26.05 tag's source:
        --no-python --no-imaging --no-examples --no-tutorials --no-tools
        --build-monolithic --onetbb --build-variant release
        --build <dir> --src <dir>
    Notes:
      * The disable flags give the core-only build the spec calls for
        (PXR_ENABLE_PYTHON_SUPPORT=OFF, PXR_BUILD_IMAGING=OFF), and
        --build-monolithic sets PXR_BUILD_MONOLITHIC=ON -> usd_ms.dll.
      * oneTBB is build_usd.py's default; --onetbb is passed explicitly to
        record the ADR-003 decision (oneTBB -> tbb12.dll, matching the
        .gdextension [dependencies]).
      * release is also the default variant; passed explicitly for determinism.
      * --build/--src default to <install_dir>/build and /src. We redirect them
        to sibling dirs so usd-install/ holds only the install output, which
        keeps the stage-2 link (task 0.5) unambiguous.

    Because build_usd.py looks for cl.exe directly on PATH, this script first
    initializes the VS 2022 (17.x) MSVC x64 environment via vcvars64.bat -- the
    same toolset the stage-2 extension build uses -- so it runs from any shell
    without a "Native Tools" prompt. (CMake's VS generator finds MSVC itself;
    build_usd.py does not.)

    See docs/building.md (Stage 1).

.PARAMETER UsdRef
    OpenUSD git tag to build. Pinned default per spec section 3.

.PARAMETER Generator
    CMake generator passed to build_usd.py. Defaults to the same generator the
    extension build uses; override if your VS install differs.

.PARAMETER VsInstallPath
    Visual Studio install root used to initialize the MSVC environment. Empty
    (default) auto-detects VS 2022 (17.x) with the C++ toolset via vswhere.

.PARAMETER Clean
    Delete any existing OpenUSD checkout, build, download, and install dirs
    before building.

.EXAMPLE
    pwsh scripts/build_usd.ps1
.EXAMPLE
    pwsh scripts/build_usd.ps1 -Clean
#>

[CmdletBinding()]
param(
    [string]$UsdRef        = "v26.05",
    [string]$Generator     = "Visual Studio 17 2022",
    [string]$VsInstallPath = "",
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# --- Paths (everything lives under the gitignored thirdparty/ dir) ----------
$RepoRoot    = Split-Path -Parent $PSScriptRoot
$ThirdParty  = Join-Path $RepoRoot   "thirdparty"
$UsdSrc      = Join-Path $ThirdParty "OpenUSD"        # pinned checkout (has build_usd.py)
$InstallDir  = Join-Path $ThirdParty "usd-install"    # final install output only
$BuildDir    = Join-Path $ThirdParty "usd-build"      # build_usd.py --build (intermediate)
$DownloadDir = Join-Path $ThirdParty "usd-src"        # build_usd.py --src  (dependency sources)
$BuildScript = Join-Path $UsdSrc     "build_scripts/build_usd.py"
$UsdRepoUrl  = "https://github.com/PixarAnimationStudios/OpenUSD.git"

# --- Helpers ----------------------------------------------------------------
function Assert-Tool {
    param([string]$Name, [string]$Hint)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required tool '$Name' is not on PATH. $Hint (See docs/building.md.)"
    }
}

function Initialize-MsvcEnvironment {
    param([string]$VsInstallPath)

    # build_usd.py checks for cl.exe directly on PATH (unlike CMake's "Visual
    # Studio" generator, which finds MSVC itself). So we initialize the MSVC
    # environment here, pinned to VS 2022 (17.x) to match the toolset the
    # stage-2 extension build uses -- mixing toolsets across the DLL boundary
    # risks a C++ ABI / CRT mismatch.
    if (-not $VsInstallPath) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        if (-not (Test-Path $vswhere)) {
            throw "vswhere.exe not found; cannot locate Visual Studio. Install VS 2022 with the C++ workload, or pass -VsInstallPath. (See docs/building.md.)"
        }
        $VsInstallPath = & $vswhere -version "[17.0,18.0)" -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath | Select-Object -First 1
        if (-not $VsInstallPath) {
            throw "No Visual Studio 2022 (17.x) install with the C++ toolset found. Install the 'Desktop development with C++' workload, or pass -VsInstallPath. (See docs/building.md.)"
        }
    }

    $vcvars = Join-Path $VsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        throw "vcvars64.bat not found at '$vcvars'. Check the VS C++ install. (See docs/building.md.)"
    }

    Write-Host ">> initializing MSVC x64 environment from: $VsInstallPath" -ForegroundColor Cyan
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }

    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $cl) {
        throw "vcvars64.bat ran but cl.exe is still not on PATH. (See docs/building.md.)"
    }
    Write-Host "   cl.exe : $($cl.Source)" -ForegroundColor DarkGray
}

# --- Go ---------------------------------------------------------------------
Write-Host "godot-usd-bridge :: OpenUSD build (stage 1)" -ForegroundColor Green
Write-Host "  pinned ref : $UsdRef"
Write-Host "  source     : $UsdSrc"
Write-Host "  install    : $InstallDir"
Write-Host ""

# build_usd.py shells out to all three of these.
Assert-Tool git    "Install Git."
Assert-Tool python "Install Python 3 and ensure 'python' is on PATH."
Assert-Tool cmake  "Install CMake >= 3.26."

# build_usd.py needs cl.exe on PATH; set up the VS 2022 MSVC env from any shell.
Initialize-MsvcEnvironment -VsInstallPath $VsInstallPath

if ($Clean) {
    foreach ($p in @($UsdSrc, $InstallDir, $BuildDir, $DownloadDir)) {
        if (Test-Path $p) {
            Write-Host ">> removing $p" -ForegroundColor Cyan
            Remove-Item -Recurse -Force $p
        }
    }
}

New-Item -ItemType Directory -Force -Path $ThirdParty | Out-Null

# Clone OpenUSD at the pinned tag (shallow), unless it's already there.
if (-not (Test-Path $BuildScript)) {
    Write-Host ">> cloning OpenUSD $UsdRef (shallow)" -ForegroundColor Cyan
    git clone --depth 1 --branch $UsdRef $UsdRepoUrl $UsdSrc
    if ($LASTEXITCODE -ne 0) { throw "git clone of OpenUSD $UsdRef failed." }
} else {
    Write-Host ">> reusing existing checkout at $UsdSrc (use -Clean to re-clone)" -ForegroundColor DarkGray
}

# The long pole: build USD + oneTBB. Streams build_usd.py output to the console.
Write-Host ">> building OpenUSD (core-only, monolithic, no Python) - this is slow" -ForegroundColor Cyan
$buildArgs = @(
    $BuildScript
    "--no-python"
    "--no-imaging"
    "--no-examples"
    "--no-tutorials"
    "--no-tools"
    "--build-monolithic"
    "--onetbb"
    "--build-variant", "release"
    "--generator", $Generator
    "--build", $BuildDir
    "--src",   $DownloadDir
    $InstallDir
)
python @buildArgs
if ($LASTEXITCODE -ne 0) { throw "build_usd.py failed (exit code $LASTEXITCODE)." }

# Sanity-check the artifacts the extension build (stage 2) will link against.
# The exact install subdir isn't asserted here (it's confirmed in task 0.5),
# so search rather than hard-code a path.
$monolithic = Get-ChildItem -Path $InstallDir -Recurse -Filter "usd_ms.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
$tbb        = Get-ChildItem -Path $InstallDir -Recurse -Filter "tbb12.dll"  -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $monolithic) { throw "Build finished but usd_ms.dll was not found under $InstallDir." }
if (-not $tbb)        { Write-Warning "tbb12.dll not found under $InstallDir - verify the oneTBB build." }

Write-Host ""
Write-Host "OpenUSD build complete." -ForegroundColor Green
Write-Host "  usd_ms.dll : $($monolithic.FullName)"
if ($tbb) { Write-Host "  tbb12.dll  : $($tbb.FullName)" }
Write-Host ""
Write-Host "Next: stage 2 (the extension build) links against $InstallDir. See docs/building.md." -ForegroundColor Green
