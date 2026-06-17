# godot-usd-bridge

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Godot 4.6+](https://img.shields.io/badge/Godot-4.6%2B-478CBF?logo=godotengine&logoColor=white)
![Status: early development](https://img.shields.io/badge/status-early%20development-orange)

Native [OpenUSD](https://openusd.org/) import for the [Godot Engine](https://godotengine.org/), packaged as a C++ [GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html). Built against the real OpenUSD library with prebuilt Windows binaries and CI as first-class goals.

> [!WARNING]
> **Early development.** There are no released binaries yet, and the public API, scene mapping, and on-disk expectations will change without notice. See the [roadmap](#roadmap) for current status.

## Goals

godot-usd-bridge translates USD stages into native Godot scene trees:

- **Geometry** — `.usd` / `.usda` / `.usdc` stages into `Node3D` hierarchies and `MeshInstance3D` / `ArrayMesh`, with n-gon triangulation, `faceVarying` primvar handling (vertex splitting for normals and UVs), winding flips, and unit / up-axis normalization.
- **Materials** — `UsdPreviewSurface` → `StandardMaterial3D`, textures resolved through USD's asset-resolution layer (correct sRGB / linear handling), and `GeomSubset`s mapped to per-surface materials.
- **Scene features** — variant-set and payload import options, scenegraph instancing, and `PointInstancer` → `MultiMeshInstance3D`.

Two ways in: a GDScript-callable runtime loader first, then editor integration so a `.usd` dropped into the FileSystem dock imports as a `PackedScene`, the way glTF does.

**Not in the current cycle:** export (Godot → USD), a Hydra render delegate, `UsdSkel` / animation, and MaterialX. Full scope and rationale are in the [spec](docs/godot-usd-bridge-spec.md).

## Roadmap

The project ships in milestones (mapping to the [spec](docs/godot-usd-bridge-spec.md)):

- [ ] **M0 — Walking skeleton.** The extension loads in the Godot editor on Windows, opens a `.usda`, and prints the prim tree — locally and in CI.
- [ ] **v0.1 — Core geometry.** Xform hierarchy, mesh import with `faceVarying` vertex splitting, winding / unit / up-axis normalization. The Pixar Kitchen Set loads with correct geometry, orientation, and scale.
- [ ] **v0.2 — Materials & subsets.** `UsdPreviewSurface` → `StandardMaterial3D`, textures via Ar, `GeomSubset`s → surfaces.
- [ ] **v0.3 — Editor integration.** FileSystem-dock import, variant / payload / scale options, instancing. Tagged release with a prebuilt Windows addon zip.

Further out: Linux/macOS binaries, AssetLibrary submission, export, `UsdSkel`, Hydra preview.

## Requirements

- **Godot 4.6+** — the extension targets the 4.6 GDExtension API (`compatibility_minimum = 4.6`).
- **Windows x86_64** is the primary platform. Linux is planned (for CI parity); macOS on demand.
- **Build toolchain:** MSVC (Visual Studio 2022) and CMake ≥ 3.26.

## Building

> [!NOTE]
> Build tooling lands in M0; detailed instructions and troubleshooting will live in `docs/building.md`.

The build is two stages — compile OpenUSD once, then build the extension against it:

```sh
# 1. Clone with submodules (pulls godot-cpp)
git clone --recursive <repo-url>
cd godot-usd-bridge

# 2. Build the pinned OpenUSD (core-only, monolithic) into thirdparty/usd-install/
pwsh scripts/build_usd.ps1

# 3. Configure and build the extension
cmake --preset windows-release
cmake --build --preset windows-release
```

The CRT (`/MD`), exceptions, and RTTI settings OpenUSD requires are pinned in the CMake presets — build through the presets rather than invoking the compiler directly.

## Usage

> [!NOTE]
> Target API — see the [roadmap](#roadmap) for what's implemented today.

Runtime loader (v0.1):

```gdscript
var root: Node3D = UsdStageLoader.load("res://assets/kitchen.usd")
add_child(root)
```

Editor import (v0.3): drop a `.usd`, `.usda`, or `.usdc` file into the FileSystem dock and it imports as a `PackedScene`, with USD-specific options (variant selection, payload policy, scale) in the import dock.

## Development workflow

The translation core (`src/translate/`) is written by hand. AI assistance (Claude Code) is used for build scripting, CI, and continuous code review, under repository rules that keep it out of that core; the level of assistance is recorded per commit via an `AI-Assist:` git trailer. The full policy is in [docs/godot-usd-bridge-ai-policy.md](docs/godot-usd-bridge-ai-policy.md).

## Documentation

- [Spec & task breakdown](docs/godot-usd-bridge-spec.md) — pinned versions, milestones, architecture, and decisions.
- [AI-assisted development policy](docs/godot-usd-bridge-ai-policy.md) — the tiering and disclosure model behind the workflow above.
- [Knowledge base](docs/godot-usd-bridge-knowledge-base.md) — plain-language reference for the USD, Godot, and build concepts the project touches.

## License

MIT — see [LICENSE](LICENSE).

Released binaries bundle [OpenUSD](https://github.com/PixarAnimationStudios/OpenUSD) and [oneTBB](https://github.com/uxlfoundation/oneTBB) (both Apache-2.0-family). Their license and `NOTICE` texts are redistributed alongside the binaries in the release package.

## Acknowledgments

Built on OpenUSD, [godot-cpp](https://github.com/godotengine/godot-cpp), oneTBB, and [GUT](https://github.com/bitwes/Gut). Earlier USD-in-Godot explorations — notably the [`meshula/usd-godot`](https://github.com/meshula/usd-godot) prototype — helped validate feasibility; this project's focus is a released, Windows-first, editor-integrated importer. See the [spec](docs/godot-usd-bridge-spec.md) for full positioning.

## Contributing

This is early and moving quickly — expect structure and APIs to churn. Issues and discussion are welcome; please open an issue before starting significant work. Commits follow [Conventional Commits](https://www.conventionalcommits.org/).
