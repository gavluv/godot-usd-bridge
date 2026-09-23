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
	"nested_xforms"              = @(
		# Synthetic stage-root node above the prim nodes.
		"- nested_xforms_usda  path=/ type=",
		# LOCAL transforms, not world: world space would put Child at (10, 5, 0).
		"- Child  path=/Root/Child type=Xform origin=(0.0000, 5.0000, 0.0000) scale=(2.0000, 2.0000, 2.0000)",
		"- Leaf  path=/Root/Child/Leaf type=Xform origin=(0.0000, 0.0000, 0.0000)",
		"euler_deg=(-0.0000, 90.0000, 0.0000)",
		"nodes: 4"
	)
	"reset_xform_stack"          = @(
		# The root correction lives on the synthetic root, so prim nodes keep
		# their USD-authored local transforms unmodified.
		"- reset_xform_stack_usda  path=/ type= origin=(0.0000, 0.0000, 0.0000) scale=(0.0100, 0.0100, 0.0100)",
		"- Root  path=/Root type=Xform origin=(10.0000, 0.0000, 0.0000) scale=(1.0000, 1.0000, 1.0000)",
		# A top-level node ignores ancestors, so it must pre-compose the
		# correction itself, or Detached lands at z=5, 100x too large.
		"- Detached  path=/Root/Detached type=Xform origin=(0.0000, 0.0000, 0.0500) scale=(0.0100, 0.0100, 0.0100) euler_deg=(-0.0000, 0.0000, 0.0000) top_level=true",
		"nodes: 4"
	)
	"no_default_prim"            = @(
		# No defaultPrim to fall back from, and multiple top-level prims: all of
		# them must survive the import.
		"- no_default_prim_usda  path=/ type=",
		"- First  path=/First",
		"- SecondChild  path=/Second/SecondChild",
		"nodes: 4"
	)
	"inactive_prims"             = @(
		# Default predicate filtering: inactive / abstract / undefined prims are
		# excluded. Unfiltered traversal would give 6 nodes.
		"- Visible  path=/Root/Visible",
		"nodes: 3"
	)
	"quad_mesh"                  = @(
		# Fan triangulation of a single quad: 4 corners -> 2 triangles.
		"mesh='Quad' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"aabb_pos=(-1.0000, 0.0000, -1.0000) aabb_size=(2.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"shared_edge_quads"          = @(
		# Non-identity faceVertexIndices: corner offsets and point indices
		# diverge from entry 6 on, so this catches conflating the two. Corner
		# offsets would be [.. 4,5,6, 4,6,7]; point indices differ.
		"mesh='SharedEdgeQuads' surfaces=1 surf0[verts=6 indices=12 tris=4] idx=[0, 1, 2, 0, 2, 3, 3, 2, 4, 3, 4, 5]",
		"aabb_pos=(-2.0000, 0.0000, -1.0000) aabb_size=(4.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"degenerate_mesh"            = @(
		# Valid but triangle-less: no surface is added, and it is a WARNING
		# (legal USD) rather than an error. Keep the WARNING: prefix in the
		# needle -- log level is the only thing separating this fixture from
		# malformed_mesh, which reaches an identical node state.
		"WARNING: USD mesh conversion warnings: /Root/AllDegenerate (no geometry; degenerate faces)",
		"- AllDegenerate  path=/Root/AllDegenerate type=Mesh",
		"mesh=<none>",
		"nodes: 3"
	)
	"malformed_mesh"             = @(
		# Out-of-range point index: conversion fails loudly, but the node is
		# still created so the prim keeps its place in the hierarchy. The
		# parenthesised reason pins WHICH check rejected it -- without it this
		# needle also matches truncated_face_indices' failure.
		"ERROR: Failed to convert USD mesh to Godot mesh: /Root/BadIndices (Point index out of range)",
		"- BadIndices  path=/Root/BadIndices type=Mesh",
		"mesh=<none>",
		"nodes: 3"
	)
	"truncated_face_indices"     = @(
		# faceVertexCounts declares 5 corners against 4 authored indices. The
		# degenerate second face means triangulation never emits an offset past
		# the end, so the per-corner bound in map_corners_to_points stays quiet
		# -- only validate_topology's sum check rejects this file.
		#
		# The only fixture that both warns and fails. Warnings print regardless
		# of outcome, so a failed conversion still reports what it worked out
		# on the way down; losing the WARNING line here means that regressed.
		"WARNING: USD mesh conversion warnings: /Root/TruncatedIndices (degenerate faces)",
		"ERROR: Failed to convert USD mesh to Godot mesh: /Root/TruncatedIndices (Corner index out of range)",
		"- TruncatedIndices  path=/Root/TruncatedIndices type=Mesh",
		"mesh=<none>",
		"nodes: 3"
	)
	# ---- channel defects (task 1.4) -------------------------------------
	# Every needle below is pinned from the opening '(' to the closing ')'.
	# Warning segments can only appear INSIDE those parens, so the closing
	# one is what makes each of these detect an EXTRA warning as well as a
	# missing one. Do not trim them to the message text alone.
	"bogus_interp"               = @(
		# An interpolation token USD does not define. There is no safe default
		# to fall back on -- the token decides both how many values to expect
		# and which one each corner gets -- so the channel is dropped.
		"WARNING: USD mesh conversion warnings: /Root/BogusInterp ([UVS] invalid interpolation mode)",
		"mesh='BogusInterp' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"nodes: 3"
	)
	"oversized_primvar"          = @(
		# More values than the interpolation calls for is USABLE: every corner
		# still resolves and the surplus is ignored. Must say 'unused', not
		# 'truncated', and must NOT drop the channel.
		"WARNING: USD mesh conversion warnings: /Root/OversizedPrimvar ([UVS] unused values)",
		"mesh='OversizedPrimvar' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"nodes: 3"
	)
	"elementsize_primvar"        = @(
		# elementSize is a stride, so 8 values over 4 points is well formed USD
		# rather than a surplus. Ignoring elementSize lands this in the 'unused
		# values' branch instead and assigns stride garbage to half the mesh.
		"WARNING: USD mesh conversion warnings: /Root/ElementSizePrimvar ([UVS] unexpected element size)",
		"mesh='ElementSizePrimvar' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"nodes: 3"
	)
	"no_normals_mesh"            = @(
		# Authors no normals, which is ordinary USD. The mesh-level warning
		# exists only to give the needle a parenthesised list to pin: a
		# [NORMALS] or [UVS] segment can only appear inside it. This is the
		# regression test for warning on every mesh that leaves normals to the
		# renderer.
		"WARNING: USD mesh conversion warnings: /Root/NoNormals (degenerate faces)",
		"mesh='NoNormals' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"nodes: 3"
	)
	"zero_normal_mesh"           = @(
		# Structurally perfect channel with one unusable VALUE. Warns and is
		# KEPT, with (0, 1, 0) substituted -- the opposite disposition from the
		# count-mismatch fixtures, which warn and drop.
		"WARNING: USD mesh conversion warnings: /Root/ZeroNormal ([NORMALS] contains zero-length normal value)",
		"mesh='ZeroNormal' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"nodes: 3"
	)
	"normals_precedence"         = @(
		# Malformed primvars:normals with a usable `normals` attribute behind
		# it. Once the fallback succeeds the specific primvar warning is
		# REPLACED, so 'invalid interpolation mode' appearing here is a failure
		# even though the primvar really did have one.
		"WARNING: USD mesh conversion warnings: /Root/NormalsPrecedence ([NORMALS] malformed normals primvar)",
		"mesh='NormalsPrecedence' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"nodes: 3"
	)
	"blocked_normals"            = @(
		# `= None` is an authored opinion, so unlike no_normals_mesh it must
		# warn. Get() fails on a block, so no channel is built and the count
		# check is never reached: 'no authored values', never 'truncated'.
		"WARNING: USD mesh conversion warnings: /Root/BlockedNormals ([NORMALS] no authored values)",
		"mesh='BlockedNormals' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"nodes: 3"
	)
	"empty_normals_array"        = @(
		# `= []` is a real value that Get() SUCCEEDS on, so a channel is built
		# and then judged short against P=4. The only fixture reaching
		# validate_channel through normals rather than a primvar, and the only
		# one covering the short-array direction.
		"WARNING: USD mesh conversion warnings: /Root/EmptyNormalsArray ([NORMALS] truncated values)",
		"mesh='EmptyNormalsArray' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"nodes: 3"
	)
	# ---- vertex splitting and compaction (task 1.4) ---------------------
	# The dump prints no UV or normal values, so every channel-VALUE claim
	# lives in the doctests. What these four add is the AABB: Godot computes
	# it over the whole ARRAY_VERTEX rather than the vertices the index array
	# reaches, so it is unreachable from MeshData and this is its only
	# coverage.
	"fv_uv_seam_quads"           = @(
		# A faceVarying UV seam: corners 3/4 and 2/5 disagree, so points 3 and
		# 2 each split, appending vertices 6 and 7. Those appended vertices
		# duplicate existing positions, so the AABB must NOT move -- a splitter
		# that appended wrong positions keeps the vertex count right and shows
		# up only here.
		"mesh='FvUvSeamQuads' surfaces=1 surf0[verts=8 indices=12 tris=4] idx=[0, 1, 2, 0, 2, 3, 6, 7, 4, 6, 4, 5]",
		"aabb_pos=(-2.0000, 0.0000, -1.0000) aabb_size=(4.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"fv_uv_seam_orphan_quads"    = @(
		# The seam fixture plus two unreferenced points: splitting appends past
		# point_count while compaction removes slots inside it, the only
		# fixture that does both. Needles are identical to fv_uv_seam_quads'
		# apart from the mesh name, and that sameness IS the assertion --
		# authored points that no face uses must leave no trace. Do not
		# collapse the two entries.
		"mesh='FvUvSeamOrphanQuads' surfaces=1 surf0[verts=8 indices=12 tris=4] idx=[0, 1, 2, 0, 2, 3, 6, 7, 4, 6, 4, 5]",
		"aabb_pos=(-2.0000, 0.0000, -1.0000) aabb_size=(4.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"fv_uv_shared_quads"         = @(
		# The same topology and interpolation as fv_uv_seam_quads with UVs that
		# AGREE across the shared edge, so nothing may split. 6 verts rather
		# than 8 is the whole assertion; the AABB matches its seam twin and
		# carries nothing on its own. Catches a splitter that emits a vertex
		# per corner whenever a faceVarying channel is present, which renders
		# correctly and is invisible by eye.
		"mesh='FvUvSharedQuads' surfaces=1 surf0[verts=6 indices=12 tris=4] idx=[0, 1, 2, 0, 2, 3, 3, 2, 4, 3, 4, 5]",
		"aabb_pos=(-2.0000, 0.0000, -1.0000) aabb_size=(4.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"partial_orphan_mesh"        = @(
		# One real face and one degenerate one, so the mesh converts while
		# still carrying orphan points. The AABB is the detector: unclaimed
		# slots are zero-initialised and the quad sits at x,z in [1, 3], so an
		# uncompacted surface reaches (0, 0, 0) and the bound becomes
		# (0, 0, 0) to (3, 0, 3). Godot bounds the whole ARRAY_VERTEX, so this
		# is the culling bound too.
		"WARNING: USD mesh conversion warnings: /Root/PartialOrphan (degenerate faces)",
		"mesh='PartialOrphan' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"aabb_pos=(1.0000, 0.0000, 1.0000) aabb_size=(2.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"indexed_primvars_quads"     = @(
		# Indexed primvars -- the only fixture where ComputeFlattened has real
		# expansion to do. The index array is identical to fv_uv_seam_quads',
		# so the VERTEX COUNT is what carries this entry: st and normals each
		# own one of the two splits, so a channel that fails to flatten is
		# dropped and the count falls to 7, with a different idx depending on
		# which one. Losing both gives 6. Do not "simplify" the fixture's
		# index arrays -- overlapping splits would mask either failure at 8.
		"mesh='IndexedPrimvarsQuads' surfaces=1 surf0[verts=8 indices=12 tris=4] idx=[0, 1, 2, 0, 2, 3, 6, 7, 4, 6, 4, 5]",
		"aabb_pos=(-2.0000, 0.0000, -1.0000) aabb_size=(4.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	# ---- UV set resolution (task 1.4) -----------------------------------
	# Two of the four UV-resolution fixtures have no entry here, deliberately.
	# float2_st_quad and uv_fallback_after_bad_candidate_quad both assert the
	# ABSENCE of a warning on a successful import, and neither failure mode
	# moves any geometry -- so every needle they could carry would still match
	# while the bug was present. Both are doctest-only; their docs say why.
	"uv_set_by_type_quads"       = @(
		# A texCoord2f[] UV set named 'UVMap' rather than 'st', so the type
		# scan is the only thing that can reach it. Needles are identical to
		# fv_uv_seam_quads' apart from the mesh name: a set found by TYPE has
		# to be processed exactly like one found by NAME -- flattened, sampled
		# per corner, V-flipped, and allowed to split.
		#
		# Without the scan this mesh imports with no UVs AND no warning, since
		# HasPrimvar('st') is false and the channel is never built. The vertex
		# count is what surfaces it: 6 rather than 8, because with no UV
		# channel in the key the seam stops splitting.
		"mesh='UvSetByTypeQuads' surfaces=1 surf0[verts=8 indices=12 tris=4] idx=[0, 1, 2, 0, 2, 3, 6, 7, 4, 6, 4, 5]",
		"aabb_pos=(-2.0000, 0.0000, -1.0000) aabb_size=(4.0000, 0.0000, 2.0000)",
		"nodes: 3"
	)
	"malformed_st_fallback_quad" = @(
		# A primary UV set rejected on elementSize with a usable 'UVMap' behind
		# it -- the only producer of BAD_PRIMARY_UVS_PRIMVAR anywhere in the
		# corpus, so without this entry that enumerator has no test.
		#
		# The warning is REPLACED, not accumulated. 'unexpected element size'
		# is raised against st first, then cleared once the fallback succeeds,
		# because by then it describes a source the importer did not use. An
		# implementation that ORs instead reports both, and the closing ')'
		# is what catches it.
		"WARNING: USD mesh conversion warnings: /Root/MalformedStFallbackQuad ([UVS] malformed primary UV set primvar)",
		"mesh='MalformedStFallbackQuad' surfaces=1 surf0[verts=4 indices=6 tris=2] idx=[0, 1, 2, 0, 2, 3]",
		"aabb_pos=(-1.0000, 0.0000, -1.0000) aabb_size=(2.0000, 0.0000, 2.0000)",
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

# Runs Godot, writes its combined output to $LogPath, returns the exit code.
#
# Redirection happens at the PROCESS level, not through the PowerShell pipeline.
# `& $exe @args 2>&1` wraps every stderr line in a NativeCommandError record, and
# rendering that record hard-wraps at the host's console width -- so a WARNING
# longer than ~120 columns lands in the log split across two lines and no
# per-line substring match can ever find it. The width is the HOST's, so the same
# needle can pass locally and fail in CI. Start-Process hands the streams
# straight to files and none of that applies; it also keeps the "At <script>:N
# char:M" and "+ CategoryInfo" decoration out of the logs.
#
# Cost: stdout and stderr are concatenated rather than interleaved. Nothing here
# depends on their relative order -- Get-MissingExpectations matches per line.
function Invoke-Godot {
	param([string[]]$GodotArgs, [string]$LogPath)

	# Start-Process joins ArgumentList with spaces and quotes nothing, so any
	# argument containing a space has to arrive pre-quoted.
	$quoted = @($GodotArgs | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } })

	$outFile = "$LogPath.out"
	$errFile = "$LogPath.err"
	$proc = Start-Process -FilePath $script:GodotExe -ArgumentList $quoted -RedirectStandardOutput $outFile `
		-RedirectStandardError $errFile -Wait -PassThru -NoNewWindow
	Get-Content $outFile, $errFile -ErrorAction SilentlyContinue | Set-Content $LogPath -Encoding utf8
	Remove-Item $outFile, $errFile -ErrorAction SilentlyContinue
	return $proc.ExitCode
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
}
elseif ((Test-Path $extensionList) -and (-not $Force)) {
	Write-Phase "warm-up (already registered)"
}
else {
	Write-Phase "warm-up"
	$log = Join-Path $LogDir "warmup.log"
	# The editor pass can exit nonzero on a bare machine even when the scan
	# completes, so its exit code is not the signal. The postcondition is.
	$code = Invoke-Godot -GodotArgs @("--headless", "--editor", "--path", $DemoDir, "--quit") -LogPath $log
	Write-Host "  editor exit code: $code (ignored; postcondition is what matters)"
	if (-not (Test-Path $extensionList)) {
		Get-Content $log
		$failures += "warm-up did not register the extension (demo/.godot/extension_list.cfg missing)"
	}
 else {
		Write-Host "  extension registered" -ForegroundColor Green
	}
}

# --- smoke -----------------------------------------------------------------
Write-Phase "smoke"
$log = Join-Path $LogDir "smoke.log"
$code = Invoke-Godot -GodotArgs @("--headless", "--path", $DemoDir) -LogPath $log
if ($code -ne 0) {
	$failures += "smoke: demo exited with code $code"
}
else {
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
		Write-Host ("  {0,-22} ok" -f $fixture) -ForegroundColor Green
	}
 else {
		Write-Host ("  {0,-22} {1} missing (see {2})" -f $fixture, $missing.Count, $log) -ForegroundColor Red
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
}
else {
	Rename-Item $usdResources "usd_disabled"
	try {
		$log = Join-Path $LogDir "negative.log"
		$code = Invoke-Godot -GodotArgs @("--headless", "--path", $DemoDir) -LogPath $log
	}
 finally {
		# Always restore, even if the run threw.
		if (Test-Path $disabled) { Rename-Item $disabled "usd" }
	}
	if ($code -eq 0) {
		$failures += "negative: demo exited 0; a missing resource tree went undetected"
	}
 else {
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
