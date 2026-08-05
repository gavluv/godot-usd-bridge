#Requires -Version 5.1
<#
.SYNOPSIS
    Applies .clang-format to the hand-written C++ sources, or checks them.

.DESCRIPTION
    Day to day you should not need this: VS Code formats on save (see
    .vscode/settings.json). Reach for it when bulk-formatting after a
    .clang-format change, or when editing outside a configured editor.

    CI runs it with -Check, so the file list and the clang-format lookup live
    in one place rather than being duplicated in the workflow.

.PARAMETER Check
    Report unformatted files and exit 1 instead of rewriting them.

.PARAMETER ClangFormat
    Path to clang-format.exe. Empty (default) searches PATH, then the Visual
    Studio LLVM component.

.EXAMPLE
    pwsh scripts/format.ps1
.EXAMPLE
    pwsh scripts/format.ps1 -Check
#>

[CmdletBinding()]
param(
	[switch]$Check,
	[string]$ClangFormat = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot

function Find-ClangFormat {
	param([string]$Explicit)

	if ($Explicit) {
		if (-not (Test-Path $Explicit)) { throw "clang-format not found at '$Explicit'." }
		return $Explicit
	}

	$onPath = Get-Command clang-format -ErrorAction SilentlyContinue
	if ($onPath) { return $onPath.Source }

	# Visual Studio ships clang-format with its LLVM component. @(...) forces an
	# array: a single match would otherwise be a string, and [0] would index its
	# first character.
	$candidates = @(Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio\2022\*\VC\Tools\Llvm\x64\bin\clang-format.exe" `
			-ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
	if ($candidates.Count -gt 0) { return $candidates[0] }

	throw "clang-format not found on PATH or in Visual Studio's LLVM component. Install LLVM or pass -ClangFormat."
}

$cf = Find-ClangFormat -Explicit $ClangFormat

Push-Location $RepoRoot
try {
	# git ls-files keeps this in sync with what is actually tracked; its
	# pathspec '*' crosses directory boundaries, so src/*.cpp covers
	# src/translate/ too.
	$files = git ls-files 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h'
	if (-not $files) { throw "No source files matched." }

	Write-Host "clang-format: $cf" -ForegroundColor DarkGray
	Write-Host "files: $($files.Count)" -ForegroundColor DarkGray

	if ($Check) {
		# --dry-run reports diagnostics for anything that would change;
		# --Werror turns those into a nonzero exit. clang-format writes those
		# diagnostics to stderr, which under $ErrorActionPreference = "Stop"
		# would abort this script before it can report cleanly, so relax it
		# just around the call.
		$previous = $ErrorActionPreference
		$ErrorActionPreference = "Continue"
		& $cf --dry-run --Werror --style=file @files 2>&1 | ForEach-Object { Write-Host $_ }
		$formatExit = $LASTEXITCODE
		$ErrorActionPreference = $previous

		if ($formatExit -ne 0) {
			Write-Host ""
			Write-Host "Unformatted files above. Fix with: pwsh scripts/format.ps1" -ForegroundColor Yellow
			exit 1
		}
		Write-Host "All files are formatted." -ForegroundColor Green
	} else {
		& $cf -i --style=file @files
		if ($LASTEXITCODE -ne 0) { throw "clang-format failed (exit $LASTEXITCODE)." }
		Write-Host "Formatted $($files.Count) files." -ForegroundColor Green
	}
} finally {
	Pop-Location
}

exit 0
