#Requires -Version 5.1
<#
.SYNOPSIS
    Generate compile_commands.json for clangd (IDE IntelliSense).

.DESCRIPTION
    Configures the `windows-tooling` preset (Ninja + CMAKE_EXPORT_COMPILE_COMMANDS)
    into build/tooling/, producing compile_commands.json -- the per-file compile
    database clangd reads to resolve USD / godot-cpp headers, completions, and
    errors. This does NOT build anything; real builds go through the VS presets.

    Rerun after adding/removing source files or changing include paths/defines
    (e.g. a new translate/ file). clangd background-indexes the delta itself.

    Mirrors build_usd.ps1's MSVC bootstrap: the Ninja generator's compiler probe
    needs cl.exe on PATH, and Ninja itself must be locatable -- both provided by
    the VS 2022 (17.x) environment set up here.

.PARAMETER VsInstallPath
    VS install root. Empty (default) auto-detects VS 2022 (17.x) via vswhere.
#>

[CmdletBinding()]
param(
	[string]$VsInstallPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot

# --- Locate VS 2022 (17.x), matching the toolset the real build uses ---------
if (-not $VsInstallPath) {
	$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
	if (-not (Test-Path $vswhere)) {
		throw "vswhere.exe not found; install VS 2022 with the C++ workload, or pass -VsInstallPath."
	}
	$VsInstallPath = & $vswhere -version "[17.0,18.0)" -products * `
		-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
		-property installationPath | Select-Object -First 1
	if (-not $VsInstallPath) {
		throw "No VS 2022 (17.x) with the C++ toolset found. Install 'Desktop development with C++', or pass -VsInstallPath."
	}
}

# --- Initialize the MSVC x64 environment (cl.exe on PATH) --------------------
$vcvars = Join-Path $VsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at '$vcvars'." }
Write-Host ">> initializing MSVC x64 environment from: $VsInstallPath" -ForegroundColor Cyan
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
	if ($_ -match '^([^=]+)=(.*)$') {
		[System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
	}
}

# --- Ensure Ninja is on PATH (bundled with VS, not added by vcvars) ----------
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
	$ninja = Get-ChildItem (Join-Path $VsInstallPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe") -ErrorAction SilentlyContinue | Select-Object -First 1
	if (-not $ninja) { throw "ninja.exe not found under the VS install; install the 'C++ CMake tools' component." }
	$env:PATH = "$(Split-Path -Parent $ninja.FullName);$env:PATH"
}

# --- Configure the tooling preset -> compile_commands.json -------------------
Write-Host ">> configuring windows-tooling (Ninja) for compile_commands.json" -ForegroundColor Cyan
Push-Location $RepoRoot
try {
	cmake --preset windows-tooling
	if ($LASTEXITCODE -ne 0) { throw "cmake configure (windows-tooling) failed." }

	# godot-cpp's bindings (gen/include: Node3D.hpp, gdextension_interface.h, ...)
	# are produced at BUILD time, not configure. Without them, clangd's database
	# points at an empty gen/include and every generated engine type fails to
	# resolve. Build just the generation target -- runs the Python generator,
	# no C++ compilation -- to populate the headers clangd reads.
	Write-Host ">> generating godot-cpp bindings (headers for clangd)" -ForegroundColor Cyan
	cmake --build build/tooling --target generate_bindings
	if ($LASTEXITCODE -ne 0) { throw "generate_bindings failed." }
} finally {
	Pop-Location
}

$cdb = Join-Path $RepoRoot "build\tooling\compile_commands.json"
if (-not (Test-Path $cdb)) { throw "Configure succeeded but $cdb was not produced." }
Write-Host ""
Write-Host "compile_commands.json ready: $cdb" -ForegroundColor Green
Write-Host "clangd (.clangd -> build/tooling) will pick it up on the next editor reload." -ForegroundColor Green
