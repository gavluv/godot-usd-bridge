#Requires -Version 5.1
<#
.SYNOPSIS
    Runs the Godot-side verification: extension load, stage import, and the
    missing-resources failure path.

.DESCRIPTION
    The same checks CI runs, in one command, so they can be exercised locally
    before pushing. CI invokes this script rather than duplicating the logic in
    YAML, so the two cannot drift.

    Four phases:
      warm-up   an editor pass, so Godot registers the .gdextension into
                demo/.godot/extension_list.cfg (a plain headless run never
                scans, and would not load the extension at all)
      smoke     runs the demo scene and checks its output
      fixtures  runs tools/dump_stage.gd over tests/fixtures/*.usda and matches
                lines that encode specific importer behaviour; see each
                fixture's own doc metadata for what it detects
      negative  renames the addon's usd/ resource tree away and requires the
                extension to fail loudly rather than silently or by crashing

    GUT (task 1.8) is expected to replace the fixture phase with real
    in-engine assertions. Until then this is the only automated check on the
    translation core's engine-side output.

    Requires a built extension: cmake --build --preset windows-debug.

.PARAMETER Godot
    Path to the Godot binary. Defaults to $env:GODOT, then `godot` on PATH.
    On Windows prefer the *_console.exe build: the GUI binary detaches from the
    console, so output capture and exit codes do not work.

.PARAMETER SkipWarmup
    Skip the editor pass. Safe once demo/.godot/extension_list.cfg exists;
    the script skips it automatically in that case anyway.

.PARAMETER Force
    Re-run the warm-up even if the extension is already registered.

.EXAMPLE
    pwsh scripts/check_godot.ps1
.EXAMPLE
    pwsh scripts/check_godot.ps1 -Godot "C:\Godot\godot_console.exe"
#>

[CmdletBinding()]
param(
	[string]$Godot = "",
	[switch]$SkipWarmup,
	[switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$DemoDir = Join-Path $RepoRoot "demo"
$FixtureDir = Join-Path $RepoRoot "tests/fixtures"
$LogDir = Join-Path $RepoRoot "build/check-logs"

# ---------------------------------------------------------------------------
# Expected output. Each entry is a substring that must appear in that fixture's
# dump. They are chosen to fail when a specific bug class regresses, not merely
# to describe current behaviour.
# ---------------------------------------------------------------------------
$FixtureExpectations = [ordered]@{
	"nested_xforms"     = @(
		# Synthetic stage-root node above the prim nodes.
		"- nested_xforms_usda  path=/ type=",
		# LOCAL transforms, not world: world space would put Child at (10, 5, 0).
		"- Child  path=/Root/Child type=Xform origin=(0.0000, 5.0000, 0.0000) scale=(2.0000, 2.0000, 2.0000)",
		"- Leaf  path=/Root/Child/Leaf type=Xform origin=(0.0000, 0.0000, 0.0000)",
		"euler_deg=(-0.0000, 90.0000, 0.0000)",
		"nodes: 4"
	)
	"reset_xform_stack" = @(
		# The root correction lives on the synthetic root, so prim nodes keep
		# their USD-authored local transforms unmodified.
		"- reset_xform_stack_usda  path=/ type= origin=(0.0000, 0.0000, 0.0000) scale=(0.0100, 0.0100, 0.0100)",
		"- Root  path=/Root type=Xform origin=(10.0000, 0.0000, 0.0000) scale=(1.0000, 1.0000, 1.0000)",
		# A top-level node ignores ancestors, so it must pre-compose the
		# correction itself, or Detached lands at z=5, 100x too large.
		"- Detached  path=/Root/Detached type=Xform origin=(0.0000, 0.0000, 0.0500) scale=(0.0100, 0.0100, 0.0100) euler_deg=(-0.0000, 0.0000, 0.0000) top_level=true",
		"nodes: 4"
	)
	"no_default_prim"   = @(
		# No defaultPrim to fall back from, and multiple top-level prims: all of
		# them must survive the import.
		"- no_default_prim_usda  path=/ type=",
		"- First  path=/First",
		"- SecondChild  path=/Second/SecondChild",
		"nodes: 4"
	)
	"inactive_prims"    = @(
		# Default predicate filtering: inactive / abstract / undefined prims are
		# excluded. Unfiltered traversal would give 6 nodes.
		"- Visible  path=/Root/Visible",
		"nodes: 3"
	)
	"quad_mesh"         = @(
		# Fan triangulation of a single quad: 4 corners -> 2 triangles.
		"mesh='Quad' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"aabb_pos=(-1.0000, 0.0000, -1.0000) aabb_size=(2.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"shared_edge_quads" = @(
		# Non-identity faceVertexIndices: corner offsets and point indices
		# diverge from entry 6 on, so this catches conflating the two. Corner
		# offsets would be [.. 4,5,6, 4,6,7]; point indices differ.
		"mesh='SharedEdgeQuads' surfaces=1 surf0[verts=6 indices=12 tris=4] idx=[0, 1, 2, 0, 2, 3, 3, 2, 4, 3, 4, 5]",
		"aabb_pos=(-2.0000, 0.0000, -1.0000) aabb_size=(4.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"degenerate_mesh"   = @(
		# Valid but triangle-less: no surface is added, and it is a WARNING
		# (legal USD) rather than an error.
		"WARNING: USD mesh has no geometry: /Root/AllDegenerate",
		"- AllDegenerate  path=/Root/AllDegenerate type=Mesh",
		"mesh=<none>",
		"nodes: 3"
	)
	"malformed_mesh"    = @(
		# Out-of-range point index: conversion fails loudly, but the node is
		# still created so the prim keeps its place in the hierarchy.
		"ERROR: Failed to convert USD mesh to Godot mesh: /Root/BadIndices",
		"- BadIndices  path=/Root/BadIndices type=Mesh",
		"mesh=<none>",
		"nodes: 3"
	)
}

$SmokeExpectations = @(
	"Registered 30 USD plugins",
	"/Root/Child/Cube (Mesh)",
	"godot-usd-bridge: pong",
	"Imported root: smoke_usda (1 children)"
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Find-Godot {
	param([string]$Explicit)

	if ($Explicit) {
		if (-not (Test-Path $Explicit)) { throw "Godot not found at '$Explicit'." }
		return (Resolve-Path $Explicit).Path
	}
	if ($env:GODOT -and (Test-Path $env:GODOT)) { return (Resolve-Path $env:GODOT).Path }

	$onPath = Get-Command godot -ErrorAction SilentlyContinue
	if ($onPath) { return $onPath.Source }

	throw "Godot not found. Set `$env:GODOT, put godot on PATH, or pass -Godot."
}

# Runs Godot, tees combined output to $LogPath, returns the exit code.
# $ErrorActionPreference is relaxed around the call: Godot writes its banner and
# errors to stderr, which would otherwise become a terminating NativeCommandError.
function Invoke-Godot {
	param([string[]]$GodotArgs, [string]$LogPath)

	$previous = $ErrorActionPreference
	$ErrorActionPreference = "Continue"
	& $script:GodotExe @GodotArgs 2>&1 | Tee-Object -FilePath $LogPath | Out-Null
	$code = $LASTEXITCODE
	$ErrorActionPreference = $previous
	return $code
}

# Returns the expectations that are missing from $LogPath. Call sites wrap this
# in @(): PowerShell unrolls returned arrays, so an empty result would come back
# as $null and .Count on it is fatal under Set-StrictMode.
function Get-MissingExpectations {
	param([string]$LogPath, [string[]]$Expected)

	$missing = @()
	foreach ($needle in $Expected) {
		if (-not (Select-String -Path $LogPath -SimpleMatch $needle -Quiet)) {
			$missing += $needle
		}
	}
	return $missing
}

function Write-Phase {
	param([string]$Name)
	Write-Host ""
	Write-Host "== $Name ==" -ForegroundColor Cyan
}

# ---------------------------------------------------------------------------
# Go
# ---------------------------------------------------------------------------
$script:GodotExe = Find-Godot -Explicit $Godot
Write-Host "godot: $script:GodotExe" -ForegroundColor DarkGray

if (-not (Test-Path (Join-Path $RepoRoot "demo/addons/godot-usd-bridge/bin"))) {
	throw "Addon binaries missing. Build first: cmake --build --preset windows-debug"
}

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$failures = @()

# --- warm-up ---------------------------------------------------------------
$extensionList = Join-Path $DemoDir ".godot/extension_list.cfg"
if ($SkipWarmup) {
	Write-Phase "warm-up (skipped)"
} elseif ((Test-Path $extensionList) -and (-not $Force)) {
	Write-Phase "warm-up (already registered)"
} else {
	Write-Phase "warm-up"
	$log = Join-Path $LogDir "warmup.log"
	# The editor pass can exit nonzero on a bare machine even when the scan
	# completes, so its exit code is not the signal. The postcondition is.
	$code = Invoke-Godot -GodotArgs @("--headless", "--editor", "--path", $DemoDir, "--quit") -LogPath $log
	Write-Host "  editor exit code: $code (ignored; postcondition is what matters)"
	if (-not (Test-Path $extensionList)) {
		Get-Content $log
		$failures += "warm-up did not register the extension (demo/.godot/extension_list.cfg missing)"
	} else {
		Write-Host "  extension registered" -ForegroundColor Green
	}
}

# --- smoke -----------------------------------------------------------------
Write-Phase "smoke"
$log = Join-Path $LogDir "smoke.log"
$code = Invoke-Godot -GodotArgs @("--headless", "--path", $DemoDir) -LogPath $log
if ($code -ne 0) {
	$failures += "smoke: demo exited with code $code"
} else {
	$missing = @(Get-MissingExpectations -LogPath $log -Expected $SmokeExpectations)
	foreach ($m in $missing) { $failures += "smoke: expected output missing: '$m'" }
	if ($missing.Count -eq 0) { Write-Host "  ok" -ForegroundColor Green }
}

# --- fixtures --------------------------------------------------------------
Write-Phase "fixtures"
foreach ($fixture in $FixtureExpectations.Keys) {
	$usd = Join-Path $FixtureDir "$fixture.usda"
	if (-not (Test-Path $usd)) {
		$failures += "fixtures: missing fixture file $usd"
		continue
	}
	$log = Join-Path $LogDir "dump-$fixture.log"
	$code = Invoke-Godot -GodotArgs @("--headless", "--path", $DemoDir, "-s", "tools/dump_stage.gd", "--", $usd) -LogPath $log
	if ($code -ne 0) {
		$failures += "fixtures/${fixture}: dump_stage.gd exited with code $code"
		continue
	}
	$missing = @(Get-MissingExpectations -LogPath $log -Expected $FixtureExpectations[$fixture])
	foreach ($m in $missing) { $failures += "fixtures/${fixture}: expected line not found: '$m'" }
	if ($missing.Count -eq 0) {
		Write-Host ("  {0,-20} ok" -f $fixture) -ForegroundColor Green
	} else {
		Write-Host ("  {0,-20} {1} missing (see {2})" -f $fixture, $missing.Count, $log) -ForegroundColor Red
	}
}

# --- negative --------------------------------------------------------------
# The task 0.5 landmine: a broken install (no usd/ resource tree) must fail
# loudly rather than silently, and must not crash.
Write-Phase "negative (missing USD resources)"
$usdResources = Join-Path $DemoDir "addons/godot-usd-bridge/usd"
$disabled = Join-Path $DemoDir "addons/godot-usd-bridge/usd_disabled"
if (-not (Test-Path $usdResources)) {
	$failures += "negative: $usdResources not present to rename; rebuild to redeploy it"
} else {
	Rename-Item $usdResources "usd_disabled"
	try {
		$log = Join-Path $LogDir "negative.log"
		$code = Invoke-Godot -GodotArgs @("--headless", "--path", $DemoDir) -LogPath $log
	} finally {
		# Always restore, even if the run threw.
		if (Test-Path $disabled) { Rename-Item $disabled "usd" }
	}
	if ($code -eq 0) {
		$failures += "negative: demo exited 0; a missing resource tree went undetected"
	} else {
		$missing = @(Get-MissingExpectations -LogPath $log -Expected @("Failed to register plugins"))
		foreach ($m in $missing) { $failures += "negative: expected error missing: '$m'" }
		if ($missing.Count -eq 0) { Write-Host "  ok (exited $code and reported the failure)" -ForegroundColor Green }
	}
}

# --- report ----------------------------------------------------------------
Write-Host ""
if ($failures.Count -gt 0) {
	Write-Host "FAILED ($($failures.Count)):" -ForegroundColor Red
	foreach ($f in $failures) { Write-Host "  - $f" -ForegroundColor Red }
	Write-Host ""
	Write-Host "Logs: $LogDir" -ForegroundColor Yellow
	exit 1
}

Write-Host "All Godot-side checks passed." -ForegroundColor Green
exit 0
