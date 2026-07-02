# Building godot-usd-bridge

> [!WARNING]
> **Early development.** This page documents what is implemented and verified
> today: the toolchain, the OpenUSD build (stage 1), the GDExtension build
> (stage 2), and verification. **Linking the extension against USD and deploying
> its resources is not implemented yet** (task 0.5); that section is marked
> `TODO`. Until it lands, the two stages build independently.

Windows x86_64 is the primary platform. Pinned versions live in
[the spec, §3](godot-usd-bridge-spec.md#3-pinned-stack); this page links there
rather than restating them, so the two can't drift.

## Prerequisites

| Tool | Version | Notes |
|---|---|---|
| Visual Studio 2022 | v143 toolset | Install the **Desktop development with C++** workload (or Build Tools 2022 + the **VCTools** workload). Provides MSVC and the Windows SDK. |
| CMake | ≥ 3.26 | `cmake --version`. An older CMake earlier on `PATH` will shadow a newer one — see Troubleshooting. |
| Python | 3.x | Used by godot-cpp's binding generator *during* the build. Must be on `PATH` as `python`. |
| Godot | pinned 4.6.x | Download the **exact** pinned patch (spec §3), not "latest". The standard build suffices; the *mono* build also works (GDScript/GDExtension don't require C#). |
| Git | any recent | Clone **recursively** for the `godot-cpp` submodule. |

> [!NOTE]
> You do **not** need a "Developer Command Prompt" — any shell works for both
> stages. Stage 2's presets use the `Visual Studio 17 2022` generator, which
> locates MSVC on its own. Stage 1's `build_usd.py` *does* require `cl.exe` on
> `PATH`, but `build_usd.ps1` initializes the MSVC environment itself
> (via `vcvars64.bat`).

```sh
git clone --recursive <repo-url>
cd godot-usd-bridge
# already cloned without --recursive?
git submodule update --init --recursive
```

## Stage 1 — Build OpenUSD &nbsp;✅

One command; run it once (and again only when the pin bumps):

```powershell
pwsh scripts/build_usd.ps1
```

The script clones OpenUSD at the pinned tag (shallow) and runs USD's own
`build_usd.py` with flags verified against that tag's source — core-only,
monolithic, Python-free, oneTBB, release:

```
--no-python --no-imaging --no-examples --no-tutorials --no-tools
--build-monolithic --onetbb --build-variant release
```

Before building, it initializes the **VS 2022 (17.x)** MSVC x64 environment via
`vcvars64.bat` (located with `vswhere`). The 17.x pin is deliberate: USD must be
compiled with the **same toolset as the extension** (stage 2's generator), or
you risk a C++ ABI / CRT mismatch across the `usd_ms.dll` boundary. If
auto-detection picks the wrong install, pass `-VsInstallPath`; `-Clean` wipes
and rebuilds from scratch.

Expect a cold build to take **tens of minutes**. Everything lands under the
gitignored `thirdparty/`:

| Directory | Contents |
|---|---|
| `thirdparty/OpenUSD/` | pinned source checkout |
| `thirdparty/usd-src/`, `usd-build/` | dependency downloads and build intermediates |
| `thirdparty/usd-install/` | **the install output — what stage 2 links against** |

### Verified install layout (v26.05)

| What | Where |
|---|---|
| `usd_ms.dll` + `usd_ms.lib` (the monolith) | `lib/` — note: **not** `bin/` |
| `tbb12.dll` (+ `tbbmalloc*.dll`) | `bin/` |
| `plugInfo.json` resource tree | `lib/usd/` — required at runtime; deployed in task 0.5 |
| CMake package config | `pxrConfig.cmake` (root), `cmake/pxrTargets.cmake` |
| Headers | `include/pxr/` |

> [!NOTE]
> `bin/` also contains 11 **MaterialX** DLLs — `usdMtlx` is part of core USD
> even with `--no-imaging`. Whether any of them are load-time dependencies of
> `usd_ms.dll` (and thus must ship in the addon) is determined in task 0.5;
> the spec's "`usd_ms.dll` + `tbb12.dll`" packaging list is not yet confirmed.

The script ends by asserting `usd_ms.dll` exists under the install dir and
printing the resolved artifact paths.

## Stage 2 — Build the extension &nbsp;✅

> [!IMPORTANT]
> Build through the **presets only** — never hand-invoke CMake with ad-hoc flags.
> The CRT (`/MD`), exceptions, RTTI, and API-version settings are load-bearing and
> pinned in [`CMakePresets.json`](../CMakePresets.json). See `CLAUDE.md`.

```sh
cmake --preset windows-debug
cmake --build --preset windows-debug
# release build:
cmake --preset windows-release
cmake --build --preset windows-release
```

The presets pin these as godot-cpp cache variables:

| Requirement | godot-cpp option (set in the presets) |
|---|---|
| Dynamic CRT (`/MD`) — to match `usd_ms.dll` | `GODOTCPP_USE_STATIC_CPP=OFF` (default `ON` ⇒ `/MT`) |
| C++ exceptions ON | `GODOTCPP_DISABLE_EXCEPTIONS=OFF` (default `ON`) |
| RTTI ON | none needed — MSVC enables `/GR` by default and godot-cpp adds no `/GR-` |
| Godot 4.6 API | `GODOTCPP_API_VERSION=4.6` (also godot-cpp's current default) |
| Debug vs release variant | `GODOTCPP_TARGET=template_debug` / `template_release` |

Output (gitignored): `addons/godot-usd-bridge/bin/libgodot-usd.windows.template_{debug,release}.x86_64.dll`.
A post-build step also copies the DLL and the `.gdextension` manifest into
`demo/addons/godot-usd-bridge/` (gitignored) so the demo project can load it.

> The first build is slow — godot-cpp generates and compiles its entire bindings
> before our small target links. Subsequent builds are incremental.

## Deploying USD resources (`plugInfo` / `PlugRegistry`) &nbsp;⛔ TODO (task 0.5)

Not yet implemented — the "known landmine" from the spec. Will cover shipping
USD's `usd/` resource tree alongside the binaries and registering it via
`PlugRegistry::RegisterPlugins()` at extension init. Written once 0.5 lands.

## Verifying the build &nbsp;✅

A `.gdextension` is registered into a Godot project's `.godot/extension_list.cfg`
during an **editor filesystem scan** — a plain headless *game* run does not scan.
So a fresh checkout needs a one-time editor warm-up before the load test:

```sh
godot --headless --editor --path demo --quit   # warm-up: registers the extension
godot --headless --path demo                    # runs the demo scene
```

Expected stdout from the second command:

```
godot-usd-bridge: pong
```

> [!NOTE]
> `--import` alone is **not** sufficient — it does not write `extension_list.cfg`.
> Use the `--editor … --quit` pass.

## Troubleshooting

- **`ERROR: C++ compiler not found` from `build_usd.py`** — it needs `cl.exe`
  on `PATH` (unlike stage 2). `build_usd.ps1` sets this up itself; if you see
  this, the script's VS detection failed — check that VS 2022 (17.x) with the
  C++ workload is installed, or pass `-VsInstallPath` explicitly.
- **Stage 1 picked the wrong Visual Studio** — the script constrains `vswhere`
  to 17.x on purpose; building USD with a newer/older toolset than the
  extension invites ABI/CRT mismatches. Point `-VsInstallPath` at the same VS
  install the stage-2 generator uses.
- **`Identifier "UsdBridge" not declared in the current scope`** — the extension
  wasn't loaded: the editor warm-up was skipped, so `.godot/extension_list.cfg`
  is missing. Run the `--editor … --quit` pass, then the game run.
- **The `pong` line looks missing** — Godot's `print()` writes to **stdout**, the
  version banner to **stderr**; some shells surface only one. Capture stdout
  explicitly (PowerShell: `Start-Process … -RedirectStandardOutput <file>`).
- **`cmake --version` still < 3.26 after upgrading** — an older install is earlier
  on `PATH` and shadows the new one. Remove it (Apps & Features) or fix `PATH`,
  then open a new shell.
- **CMake error reading presets that mentions `extern/godot-cpp`** — a stale CMake
  (< 3.25) can't parse this repo's `version: 6` presets file; upgrade to ≥ 3.26.
- **Confirm `/MD` actually took** — `dumpbin /dependents` (from a VS toolset shell)
  on the built DLL should list `MSVCP140.dll` / `VCRUNTIME140.dll`. Their absence
  means a static CRT slipped in.
- **No `compile_commands.json`** — expected: the `Visual Studio 17 2022` generator
  doesn't emit one. Verify compile flags via MSBuild verbosity instead, or add a
  Ninja preset later (planned for CI, task 0.7).
