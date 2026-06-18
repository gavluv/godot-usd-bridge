# Godot USD Bridge — Spec & Task Breakdown (v2)

**Status:** Active · **Updated:** June 2026

A C++ GDExtension adding native OpenUSD import to Godot 4.x. Windows is the primary development and target platform. Runtime loader first, editor integration second. Goal: a production-quality, maintained native USD importer for the Godot community — real OpenUSD, shipped with prebuilt binaries and CI.

---

## 1. Landscape & Positioning (verified June 2026)

The original premise ("Godot has no USD support") holds in spirit but needs precision:

| Prior art | What it is | Why the gap remains |
|---|---|---|
| `meshula/usd-godot` | Native GDExtension prototype by Nick Porcino (Pixar/USD ecosystem). Runtime stage manipulation from GDScript, import/export, variants. | macOS-centric (.dylib), ~23 commits, no releases, no editor import pipeline. Validates feasibility; its `docs/embeddingUSD.md` is useful reference. |
| `V-Sekai/godot-usd` | Addon that round-trips through Blender. | Not native — requires Blender installed; no USD API access. |
| `godot-proposals` #7744, discussions #5469/#7436 | Open proposals debating built-in USD. | Demand documented; nothing landed in core. |
| `tinyusdz` | Dependency-free C++14 USD reader. | **Explicitly rejected** for this project: incomplete composition semantics — full composition correctness requires real OpenUSD. Record this as a decision (ADR-001). |

**Differentiation statement:** the first *released, Windows-first, editor-integrated* USD importer for Godot — drag a `.usd` into the FileSystem dock and it imports like glTF, with prebuilt binaries and CI.

---

## 2. Goals & Non-Goals

**Goals**
- Import `.usd` / `.usda` / `.usdc` stages into Godot scene trees: Xforms, meshes (with correct triangulation and primvar handling), then materials, subsets, variants, instancing.
- Ship tagged releases with prebuilt Windows binaries; CI from day one.
- Acceptance-tested against real assets (Pixar Kitchen Set, USD-WG assets, Blender exports).

**Non-Goals (current cycle)**
- Hydra viewport / render delegate (meshula's angle; massive scope).
- Export (Godot → USD), UsdSkel/animation, MaterialX, physics schemas.
- macOS support (Linux comes earlier, for CI sanity; macOS only on demand).

`.usdz` read support is a cheap-ish add (OpenUSD reads the package natively; textures need buffer-loading) — parked as a v0.2 stretch, not a goal.

---

## 3. Pinned Stack

| Component | Pin | Notes |
|---|---|---|
| OpenUSD | **v26.05** (latest release) | Boost-free since 24.11. Core-only build: `PXR_ENABLE_PYTHON_SUPPORT=OFF`, `PXR_BUILD_IMAGING=OFF`, `PXR_BUILD_MONOLITHIC=ON`. Remaining hard dep: oneTBB. |
| Godot | **4.6** (develop/test on latest 4.6.x patch; 4.6.3 at decision time) | `compatibility_minimum = 4.6` in `.gdextension`. Patch releases don't change the extension API — track them freely. *(OQ-1 resolved.)* |
| godot-cpp | Submodule on a **v10/`master`** commit; targets 4.6 via `api_version` (no 4.6 branch/tag yet — re-pin to the v10 stable release branch when it ships) | Must build with **exceptions ON** (overrides godot-cpp default) and **RTTI ON**; USD requires both. |
| Toolchain | MSVC (VS 2022), CMake ≥ 3.26 | **Dynamic CRT (`/MD`) everywhere** — godot-cpp defaults to static CRT on Windows; must be overridden to match `usd_ms.dll`. |
| Build system | CMake + CMakePresets | Settled (USD is CMake-native). SCons dropped. |
| CI | GitHub Actions | `windows-latest` primary; `ubuntu-latest` added in v0.2+. USD build cached as artifact. Headless Godot downloaded at the **exact pinned 4.6.x version**, never "latest". |
| Tests | GUT (in-engine, GDScript) + golden `.usda` fixtures | Headless via `godot --headless`. C++ unit framework only if translation core grows enough to warrant it (OQ-3). |

### Linking & packaging model (v0.1 decision)

**Monolithic shared:** ship `usd_ms.dll` (+ `tbb12.dll`) alongside `libgodot-usd.dll`, declared in the `.gdextension` `[dependencies]` section. Static-linking USD into the extension DLL is deferred — USD's `TF_REGISTRY` machinery relies on static initializers that static linking can strip, and shared is the well-trodden Windows path.

**The known landmine:** even monolithic builds require USD's `plugInfo.json` resource tree (`usd/` directory) discoverable at runtime. We ship it next to the binaries and call `PlugRegistry::RegisterPlugins()` explicitly at extension init with a path resolved relative to the extension library. M0 includes an integration test that fails loudly if registration breaks.

### Build reproducibility without Docker

**Decision: no containerized build (ADR-002).** A containerized Windows build was the traditional answer to pre-24.x USD's dependency sprawl (Boost especially). The 26.x core-only build is small enough that it doesn't need one; it's handled by:

1. `scripts/build_usd.ps1` — clones OpenUSD at the pinned tag, invokes `build_usd.py` with `--no-python --no-imaging --no-examples --no-tutorials --no-tools --build-monolithic --onetbb` (verify exact flag names against the pinned tag's `--help`; `--onetbb` builds oneTBB → `tbb12.dll` to match `[dependencies]`, and confirm whether it's still opt-in vs the default), installs to `thirdparty/usd-install/` (gitignored).
2. GitHub Actions as the reproducibility layer — the USD build runs once per pin-bump and is cached.

Revisit Docker (Windows containers + VS Build Tools) only if local/CI drift becomes a real problem.

---

## 4. Architecture

```
godot-usd-bridge/
├── src/
│   ├── register_types.cpp        # GDExtension entry; PlugRegistry init
│   ├── usd_stage_loader.{h,cpp}  # Public runtime API (GDScript-callable)
│   ├── translate/                # Core translation (editor-independent)
│   │   ├── conventions.{h,cpp}   # metersPerUnit, upAxis, winding
│   │   ├── xform_import.{h,cpp}  # UsdGeomXformCache → Transform3D
│   │   ├── mesh_import.{h,cpp}   # triangulation, primvar/vertex split
│   │   └── material_import.{h,cpp}  # v0.2
│   └── editor/                   # v0.3: scene format importer plugin
├── scripts/build_usd.ps1
├── thirdparty/usd-install/       # gitignored build output
├── addons/godot-usd-bridge/      # distributable: .gdextension, dlls, usd/ resources
├── demo/                         # Godot test project + GUT tests + fixtures
├── docs/adr/                     # ADR-001 (OpenUSD over tinyusdz), ADR-002 (no Docker), ADR-003 (oneTBB over classic Intel TBB), ADR-004 (godot-cpp v10/master + api_version), ...
└── CMakeLists.txt + CMakePresets.json
```

**Two API layers, built in order:**

1. **Runtime loader (v0.1–v0.2):** `UsdStageLoader.load(path) -> Node3D` callable from GDScript. Fast to demo, zero editor plumbing, exercises the whole translation core.
2. **Editor importer (v0.3):** wraps the same core so `.usd` files import as `PackedScene` from the FileSystem dock. Preferred class: `EditorSceneFormatImporter` (the glTF/FBX pathway — inherits the standard 3D import dock). A spike task verifies its virtuals are overridable from GDExtension on the pinned Godot version; fallback is `EditorImportPlugin`.

**Convention normalization (applies to everything):**
- `metersPerUnit` (USD default 0.01) → uniform scale to Godot meters.
- `upAxis` stage metadata: Y is USD's default but Z-up stages are common (Houdini, some Blender exports) → root rotation.
- Winding: USD default orientation is `rightHanded` (CCW); Godot front faces are clockwise → reverse triangle index order (as Godot's glTF importer does). Respect per-mesh `orientation = leftHanded`.
- Prim names sanitized to valid Godot node names; prim path stashed in node metadata for traceability.

---

## 5. Milestones

### M0 — Walking Skeleton *(de-risks ~80% of project risk before any importer logic)*
**Done when:** the extension loads in the Godot editor on Windows, opens a `.usda`, and prints the prim tree (path + type) to Output — locally and in CI.

### v0.1 — Core Geometry Import
Xform hierarchy → `Node3D` tree; `UsdGeomMesh` → `MeshInstance3D`/`ArrayMesh` with n-gon triangulation, **faceVarying primvar handling via vertex splitting** (normals + `st` UVs — this is the heart of the importer, not a follow-up), winding flip, unit/axis normalization, purpose/visibility filtering (skip `guide`, skip invisible).
**Done when:** Pixar Kitchen Set loads with correct geometry, orientation, and scale (untextured is fine).

### v0.2 — Materials & Subsets
`UsdPreviewSurface` → `StandardMaterial3D` (base color, metallic, roughness, normal, emissive, opacity modes); `UsdUVTexture` resolution via Ar with correct sRGB/linear handling; **GeomSubsets → ArrayMesh surfaces** (moved up from v0.3 — real assets are multi-material); `doubleSided` → cull mode.
**Done when:** a textured USD-WG / NVIDIA sample asset renders sensibly. Stretch: `.usdz` read.

### v0.3 — Editor Integration, Variants, Instancing
Scene format importer registration; import options (variant set selections enumerated from the stage, payload load policy, scale override); scene-graph instancing → shared `ArrayMesh` resources; `PointInstancer` → `MultiMeshInstance3D` (a genuine differentiator). **Composition itself needs no work** — `UsdStage::Open` returns the composed stage; v0.3 is about *exposing* composition features, not implementing them.
**Done when:** v0.3.0 tagged release with prebuilt Windows addon zip and install docs.

### Backlog (post-v0.3)
Linux/macOS binaries · AssetLibrary submission · export · UsdSkel · time-sampled xform animation · Hydra preview · MaterialX.

---

## 6. Task Breakdown

Sizing: **S** ≤ half day · **M** 1–2 days · **L** 3+ days. Order within a milestone is the intended sequence.

### M0 — Walking Skeleton
| # | Task | Size |
|---|---|---|
| 0.1 | Repo scaffold: layout above, godot-cpp submodule pinned, .gitignore, MIT LICENSE (+ OpenUSD license text for binary redistribution), README stub | S |
| 0.2 | CMake superstructure + `CMakePresets.json` (windows-debug/release); builds a hello-world GDExtension class against godot-cpp with `/MD`, exceptions, RTTI | M |
| 0.3 | Hello extension in-engine: `UsdBridge` RefCounted with `ping()`; `.gdextension` file; demo project loads it | S |
| 0.4 | `scripts/build_usd.ps1`: pinned OpenUSD v26.05 core-only monolithic build → `thirdparty/usd-install/` | M (mostly wall-clock) |
| 0.5 | Link `usd_ms` + TBB into the extension; copy `usd/` resources into addon; `PlugRegistry::RegisterPlugins()` at init, path-relative to the extension DLL | M ⚠ *the landmine task* |
| 0.6 | Smoke test: `UsdBridge.open_stage(path)` traverses and prints prim tree; checked-in `.usda` fixture | S |
| 0.7 | CI: GitHub Actions windows job — cached USD build, extension build, headless smoke test via `godot --headless` (exact pinned 4.6.x binary) | M |

### v0.1 — Core Geometry Import
| # | Task | Size |
|---|---|---|
| 1.1 | Conventions module: read `metersPerUnit` + `upAxis`, produce root correction transform; fixture tests (Y-up cm stage, Z-up m stage) | S |
| 1.2 | Xform import: traversal with `UsdGeomXformCache`; `Xform`/`Scope` → `Node3D`; node-name sanitization; prim path in metadata | M |
| 1.3 | Mesh extraction: `points`, `faceVertexCounts`/`faceVertexIndices`; fan triangulation (document convex assumption; ear-clip later if assets demand it) | M |
| 1.4 | Primvar engine: normals (authored attr or primvar) and `st` UVs across `vertex` / `faceVarying` / `uniform` / `constant` interpolations; indexed primvars via `ComputeFlattened()`; **vertex splitting for faceVarying** | **L** — the meat |
| 1.5 | Winding & orientation: CCW→CW index flip; honor `orientation = leftHanded` | S |
| 1.6 | Purpose & visibility filtering (skip `guide` + invisible; import option later) | S |
| 1.7 | `ArrayMesh` assembly + `MeshInstance3D` wiring; first Kitchen Set load | M |
| 1.8 | Test corpus + GUT goldens: n-gon cube, faceVarying-UV quad, Z-up stage, multi-mesh hierarchy; assert vertex/index counts and AABBs | M |
| 1.9 | Error handling: Tf error marks → Godot error macros; malformed files fail gracefully, never crash the editor | M |

### v0.2 — Materials & Subsets
| # | Task | Size |
|---|---|---|
| 2.1 | Material resolution: `UsdShadeMaterialBindingAPI` bound material → locate `UsdPreviewSurface` shader; read constant inputs | M |
| 2.2 | Texture path: `UsdUVTexture` → Ar-resolved file → `ImageTexture`; sRGB (albedo/emissive) vs linear (normal/roughness/metallic); wrap modes | **L** |
| 2.3 | `GeomSubsets` (materialBind family) → multiple ArrayMesh surfaces incl. remainder subset | M |
| 2.4 | `StandardMaterial3D` mapping: opacity/alpha modes (`opacityThreshold` → scissor), `doubleSided` → cull off | M |
| 2.5 | Real-asset validation pass (USD-WG assets, NVIDIA samples); fix fallout | M |
| 2.6 | *Stretch:* `.usdz` read — textures from package via buffer loading | M |

### v0.3 — Editor Integration, Variants, Instancing
| # | Task | Size |
|---|---|---|
| 3.1 | **Spike:** confirm `EditorSceneFormatImporter` is overridable from GDExtension on pinned Godot; else fall back to `EditorImportPlugin` | S |
| 3.2 | Importer registration: `.usd/.usda/.usdc` import as `PackedScene` from FileSystem dock | **L** |
| 3.3 | Import options UI: enumerate variant sets → selection options; payload policy; scale override; purpose toggle | M |
| 3.4 | Instancing: instanceable prototypes → shared `ArrayMesh`; `PointInstancer` → `MultiMeshInstance3D` | **L** |
| 3.5 | Release engineering: versioned GitHub release, addon zip (extension DLL + `usd_ms.dll` + `tbb12.dll` + `usd/` resources), install docs, README demo GIFs | M |
| 3.6 | Per-milestone devlog/writeup | S each |

---

## 7. Risks & Mitigations

| Risk | Mitigation |
|---|---|
| USD Windows build friction (even reduced) | Pinned script + flags known-good in CI; build cached; ADR-002 keeps Docker as a documented fallback |
| plugInfo discovery silently broken in shipped addon | Explicit `PlugRegistry` registration + M0 integration test that asserts core plugins are registered |
| CRT / exceptions / RTTI mismatch causing heap corruption or link errors | All flags pinned in CMake presets; documented in `building.md`; never hand-built |
| faceVarying vertex-split bugs (subtle, asset-dependent) | Golden fixtures with exact vertex/index count assertions before touching real assets |
| DLL deployment ("works on my machine") | `.gdextension [dependencies]` declarations; CI installs the addon zip into a clean project and runs the smoke test |
| Scope creep toward Hydra/export | Non-goals section; backlog quarantine |
| `EditorSceneFormatImporter` not extension-friendly | Spike task 3.1 first; `EditorImportPlugin` fallback keeps v0.3 viable |

---

## 8. Open Questions

- **OQ-1 — RESOLVED (June 2026):** Pin Godot **4.6** (latest stable). godot-cpp at the 4.6 tag, `compatibility_minimum = 4.6`. Rationale: the pin sets the compatibility *floor*, not ceiling; 4.6 is three patch releases settled; the task 3.1 spike wants the newest GDExtension API surface. Lower the floor later only if real users ask.
- **OQ-2:** `.usdz` in v0.2 stretch or punt to backlog?
- **OQ-3:** Add a C++ unit test framework (doctest) for `translate/`, or is GUT + goldens sufficient through v0.2?