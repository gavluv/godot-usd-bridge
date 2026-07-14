# Godot USD Bridge — Knowledge Base

**Status:** Active · **Created:** June 2026 · Companion to `godot-usd-bridge-spec.md` and `godot-usd-bridge-ai-policy.md`

A shared reference — a project-scoped "Wikipedia" — for the concepts and technologies this project touches. Each entry aims to be **brief but comprehensive**: a plain-language definition, *why it matters for this project specifically*, a concrete example where useful, and links to authoritative sources for deeper reading and citation.

This doubles as a Tier-1 tutor aid under the AI policy: it explains *concepts and the "why"* (which the policy assigns to AI), and deliberately stops short of writing the learning-target implementations (triangulation, the primvar/vertex-split engine, conventions math) — those stay hand-written. Where an entry describes a Tier-1 topic, treat it as background, not a code drop.

**How to read it**
- Entries are grouped by domain (USD → Godot → C++/build → VCS/CI → graphics fundamentals). Use the table of contents to jump.
- "→ see also" lines link related entries.
- Versions/pins reflect the spec as verified June 2026; this doc cites the upstream sources so claims can be re-checked when pins bump.

---

## Table of contents

1. [OpenUSD — the domain core](#1-openusd--the-domain-core)
2. [Godot & GDExtension — the target engine](#2-godot--gdextension--the-target-engine)
3. [C++ build & toolchain — the systems layer](#3-c-build--toolchain--the-systems-layer)
4. [Version control, CI/CD & project hygiene](#4-version-control-cicd--project-hygiene)
5. [Graphics & data fundamentals — cross-cutting](#5-graphics--data-fundamentals--cross-cutting)
6. [Quick reference: the pinned stack](#6-quick-reference-the-pinned-stack)

---

## 1. OpenUSD — the domain core

### OpenUSD (Universal Scene Description)
An open-source framework, originally built at Pixar for film production and later open-sourced, for **describing, composing, and interchanging 3D scene data** — geometry, transforms, materials, lighting, and more — across many tools without lossy file conversion ([overview](https://openusd.org/release/intro.html), [NVIDIA glossary](https://www.nvidia.com/en-us/glossary/openusd/)). Standards stewardship now sits with the [Alliance for OpenUSD (AOUSD)](https://aousd.org/). The codebase lives at [github.com/PixarAnimationStudios/OpenUSD](https://github.com/PixarAnimationStudios/OpenUSD); API docs at [openusd.org](https://openusd.org/release/api/index.html).

**Why it matters here:** USD *is* the thing we're importing. Using the real OpenUSD library (not a re-implementation) is the correctness guarantee — composition semantics are genuinely hard, and only the reference implementation gets them fully right. This is recorded as **ADR-001** (OpenUSD over `tinyusdz`).
→ see also [Composition](#composition-the-livrps-arcs), [oneTBB](#onetbb), [The plugin system](#the-plugin-system-tf-plug-plugregistry-pluginfojson)

### USD file formats: `.usda` / `.usdc` / `.usd` / `.usdz`
USD is a format *family* over one data model. `.usda` is human-readable ASCII (great for fixtures and debugging). `.usdc` is the "crate" binary format (compact, fast to load). `.usd` is format-agnostic — it can hold either ASCII or binary and is resolved at open time. `.usdz` is an **uncompressed zip package** bundling a USD file plus its textures and other assets into one shippable file ([file format overview](https://openusd.org/release/glossary.html#usdz), [.usdz spec](https://openusd.org/release/spec_usdz.html)).

**Why it matters here:** We import all three of `.usd/.usda/.usdc` (OpenUSD handles the parsing transparently). `.usda` is our test-fixture format because goldens need to be diffable and hand-authorable. `.usdz` read is a v0.2 *stretch*: OpenUSD opens the package natively, but textures inside it need buffer-based loading rather than filesystem paths (**OQ-2**).
→ see also [Asset resolution (Ar)](#asset-resolution-ar)

### Stage, layers & the scenegraph
A **stage** (`UsdStage`) is the composed, runtime view of a scene you get from `UsdStage::Open(path)`. It's assembled from one or more **layers** (`.usd*` files, the `SdfLayer` objects). The composed result is a tree of **prims** (primitives) addressed by **paths** like `/World/Kitchen/Table`. Each prim has **properties** — *attributes* (typed values, possibly time-sampled) and *relationships* (typed pointers to other paths) ([glossary: Stage](https://docs.nvidia.com/learn-openusd/latest/glossary.html), [API: UsdStage](https://openusd.org/release/api/class_usd_stage.html)).

```
/World            (Xform)
└── /World/Table  (Xform)
    └── /World/Table/Top  (Mesh)
```

**Why it matters here:** Traversal of this tree is the spine of the importer — M0's smoke test just walks it and prints `path + type`. The whole translation core is "for each prim, produce the corresponding Godot node/resource."
→ see also [Godot scene tree & nodes](#the-scene-tree--core-nodes-node3d-meshinstance3d-arraymesh-packedscene), [Xforms & transforms](#xforms--transforms)

### Composition (the LIVRPS arcs)
USD's defining feature: a final scene is **composed** from layered, reusable pieces via *composition arcs*. The arcs, in strength order, are **L**ocal, **I**nherits, **V**ariantSets, **R**eferences, **P**ayloads, **S**pecializes ("LIVRPS") ([composition overview](https://openusd.org/release/glossary.html#livrps-strength-ordering)). Key arcs in practice:
- **References** — pull another asset/prim into this one (assemble a scene from parts).
- **Payloads** — like references but *deferrable*: heavy data that isn't loaded until explicitly requested ([payload](https://openusd.org/release/glossary.html#usdglossary-payload)).
- **Variants / VariantSets** — named, switchable alternatives baked into an asset (e.g. `modelingVariant = {damaged, pristine}`) ([variants](https://openusd.org/release/glossary.html#usdglossary-variant)).

**Why it matters here:** This is the single biggest "looks scary, is actually free" point. `UsdStage::Open` returns the *already-composed* stage — we never implement composition. v0.3 only *exposes* it: enumerating variant sets as import options and honoring payload load policy. Don't let composition's reputation inflate scope (it's quarantined in the spec's non-goals reasoning).
→ see also [Instancing](#instancing-scenegraph--pointinstancer)

### Prims & schemas
Every prim has a **type** defined by a **schema**. *IsA (typed) schemas* define what a prim fundamentally is — `Mesh`, `Xform`, `Camera`, `Scope` — via the `typeName` ([IsA schema](https://docs.nvidia.com/learn-openusd/latest/glossary.html)). *API schemas* (suffixed `...API`, e.g. `UsdShadeMaterialBindingAPI`) add namespaced behavior/properties without changing the prim's core type. Geometry types live in the **UsdGeom** domain; shading in **UsdShade**.

**Why it matters here:** The importer is essentially a dispatch on schema type. `Xform`/`Scope` → `Node3D`; `Mesh` → `MeshInstance3D`; bound `Material` → `StandardMaterial3D`. Reading a prim's schema correctly (via the typed C++ classes like `UsdGeomMesh(prim)`) is how we know what to translate it into.

### Xforms & transforms
A `UsdGeomXform` carries a stack of **xformOps** (translate / rotate / scale / a full matrix) under `xformOpOrder`. Because transforms compose down the hierarchy, USD provides `UsdGeomXformCache` to efficiently compute a prim's **local-to-world** (or local-to-parent) matrix while caching intermediate results ([API: UsdGeomXformCache](https://openusd.org/release/api/class_usd_geom_xform_cache.html), [Xformable](https://openusd.org/release/api/class_usd_geom_xformable.html)).

**Why it matters here:** Task 1.2. We walk the stage with an `XformCache`, convert each prim's matrix into Godot's `Transform3D`, and parent nodes accordingly. The cache matters for performance on deep hierarchies (Kitchen Set is deep). USD matrices are **row-major / row-vector**; Godot's `Transform3D` is column-major `basis + origin` — getting that mapping right is part of the Tier-1 conventions work.
→ see also [Conventions](#conventions-metersperunit-upaxis-orientation-purpose-visibility), [Coordinate systems & handedness](#coordinate-systems--handedness)

### Meshes (`UsdGeomMesh`)
A polygon mesh is defined by three core attributes: `points` (the vertex positions), `faceVertexCounts` (how many vertices each face has — `[4,4,3,...]`), and `faceVertexIndices` (flat list of indices into `points`, grouped per face by the counts) ([API: UsdGeomMesh](https://openusd.org/release/api/class_usd_geom_mesh.html)). Faces can be **n-gons** (any vertex count), not just triangles, and meshes may be polygonal or subdivision surfaces.

```usda
def Mesh "Quad" {
    point3f[] points = [(-1,0,-1), (1,0,-1), (1,0,1), (-1,0,1)]
    int[] faceVertexCounts = [4]
    int[] faceVertexIndices = [0, 1, 2, 3]
}
```

**Why it matters here:** Tasks 1.3–1.7. Godot wants triangles in an `ArrayMesh`, so we triangulate the n-gons (fan triangulation to start; document the convex assumption). This is a Tier-1 area — the explanation here is background, the implementation is hand-written.
→ see also [Polygon triangulation](#polygon-triangulation-fan--ear-clipping), [Vertices, normals, UVs & buffers](#vertices-normals-uvs--vertexindex-buffers)

### Primvars & interpolation — and vertex splitting *(the heart)*
A **primvar** ("primitive variable") is an attribute, in the `primvars:` namespace, whose values can **vary across a surface** according to an **interpolation mode** ([primvars guide](https://openusd.org/release/user_guides/primvars.html), [UsdGeomPrimvar](https://openusd.org/release/api/class_usd_geom_primvar.html)). For meshes the modes (and how many values each expects) are:

| Mode | One value per… | Example |
|---|---|---|
| `constant` | whole mesh | a single display color |
| `uniform` | face | per-face id |
| `varying` | point (linear interp) | — |
| `vertex` | point (interp per subdivision scheme) | smooth normals |
| `faceVarying` | **face-vertex** (one per index in `faceVertexIndices`) | UV seams, hard edges |

`faceVarying` is the important one: it lets a single shared point carry *different* UV or normal values depending on which face is using it — that's how you get a UV **seam** or a **hard edge** ([UsdGeomMesh interpolation notes](https://openusd.org/release/api/class_usd_geom_mesh.html)). Primvars can also be **indexed** (values stored once, referenced by an index array to save memory); `UsdGeomPrimvar::ComputeFlattened()` expands them ([ComputeFlattened](https://openusd.org/release/api/class_usd_geom_primvar.html)).

**Why it matters here — this is task 1.4, the meat of the importer.** A GPU mesh (Godot's `ArrayMesh`) has **one** attribute set per vertex, addressed by a single index buffer — it cannot represent "this point has two different UVs depending on the face." So when USD authors `faceVarying` normals/UVs, we must **split vertices**: duplicate the shared point into multiple distinct vertices, each carrying its own attributes, and rewrite the indices. Getting the dedup/split logic right (and asserting exact vertex/index counts in goldens *before* touching real assets) is the make-or-break correctness work. The fan-out across `constant/uniform/varying/vertex/faceVarying` × `normals + st` is the combinatorial part.
→ see also [Vertices, normals, UVs & buffers](#vertices-normals-uvs--vertexindex-buffers), [GeomSubsets](#geomsubsets)

### GeomSubsets
A `UsdGeomSubset` names a **subset of a mesh's faces** — most commonly with the `materialBind` family, so different regions of one mesh get different materials ([API: UsdGeomSubset](https://openusd.org/release/api/class_usd_geom_subset.html)).

**Why it matters here:** Task 2.3, moved up from v0.3 because real assets are multi-material. Each materialBind subset maps to a **separate surface** in the Godot `ArrayMesh` (Godot binds one material per surface), plus a "remainder" surface for faces in no subset. Pairs tightly with material resolution.
→ see also [Materials](#materials-usdshade-usdpreviewsurface-usduvtexture)

### Materials: UsdShade, UsdPreviewSurface, UsdUVTexture
Shading in USD is a **network of connected shader prims** in the `UsdShade` domain. `UsdPreviewSurface` is the **portable, render-agnostic PBR shader** every DCC understands (inputs: `diffuseColor`, `metallic`, `roughness`, `normal`, `emissiveColor`, `opacity`, …) ([UsdPreviewSurface spec](https://openusd.org/release/spec_usdpreviewsurface.html)). `UsdUVTexture` is the texture-reader node feeding those inputs. Binding a material to geometry uses `UsdShadeMaterialBindingAPI` ([API](https://openusd.org/release/api/class_usd_shade_material_binding_a_p_i.html)).

**Why it matters here:** v0.2. We resolve the bound material, find the `UsdPreviewSurface`, read its inputs, and map them onto Godot's `StandardMaterial3D`. The *decisions* (which input maps where, sRGB-vs-linear per channel, opacity modes) are Tier-1; the file-loading plumbing is Tier-2.
→ see also [sRGB vs linear color](#srgb-vs-linear-color), [Asset resolution (Ar)](#asset-resolution-ar)

### Instancing: scenegraph & PointInstancer
Two distinct mechanisms. **Scenegraph instancing** marks prims `instanceable` so identical referenced subtrees share a single underlying *prototype* in memory (the stage exposes them as instance proxies) ([scenegraph instancing](https://openusd.org/release/api/_usd__page__scenegraph_instancing.html)). A **PointInstancer** is a single prim that scatters many copies of one or more prototypes at authored positions/orientations/scales — think forests, crowds, bolts ([API: UsdGeomPointInstancer](https://openusd.org/release/api/class_usd_geom_point_instancer.html)).

**Why it matters here:** v0.3 (task 3.4). Scenegraph instances → shared `ArrayMesh` resources (memory win). `PointInstancer` → Godot's `MultiMeshInstance3D`, which is purpose-built for drawing thousands of copies of one mesh in one draw call — a genuine differentiator versus naïvely instantiating thousands of nodes.
→ see also [Godot scene tree & nodes](#the-scene-tree--core-nodes-node3d-meshinstance3d-arraymesh-packedscene)

### Conventions: metersPerUnit, upAxis, orientation, purpose, visibility
Stage metadata and per-prim attributes that encode *house style*, which differs between USD and Godot:
- **`metersPerUnit`** — USD's default is `0.01` (centimeters). Godot is meters. → uniform scale ([linear units](https://openusd.org/release/api/group___usd_geom_linear_units__group.html)).
- **`upAxis`** — USD defaults to **Y-up**, but Z-up stages are common (Houdini, some Blender exports) → a root rotation ([upAxis](https://openusd.org/release/api/group___usd_geom_up_axis__group.html)).
- **`orientation`** — per-mesh winding; USD default is `rightHanded` (CCW). → may need an index flip (see below).
- **`purpose`** — `default` / `render` / `proxy` / `guide`. We skip `guide` (and typically import `render`/`default`) ([purpose](https://openusd.org/release/glossary.html#usdglossary-purpose)).
- **`visibility`** — inherited `invisible` prims are skipped.

**Why it matters here:** Task 1.1, and it touches *everything* downstream — get the root correction transform wrong and the whole Kitchen Set is sideways or 100× too big. Tier-1 math; fixtures cover Y-up-cm and Z-up-m stages explicitly.
→ see also [Coordinate systems & handedness](#coordinate-systems--handedness), [Winding order & face culling](#winding-order--face-culling)

### Asset resolution (Ar)
**Ar** is USD's pluggable **asset-resolution** layer: it turns asset paths written in a stage (which may be relative, packaged, or URI-like) into something loadable ([Ar overview](https://openusd.org/release/api/ar_page_front.html)). The default resolver handles filesystem and search paths; `.usdz` packages and pipelines use custom resolvers.

**Why it matters here:** Task 2.2. A texture referenced as `./textures/wood.png` or living inside a `.usdz` must go through Ar to become a real byte stream before Godot can build an `ImageTexture`. Skipping Ar and treating paths as raw filesystem strings is the classic "works on my machine, breaks on packaged assets" bug.

### The plugin system (Tf, Plug, PlugRegistry, plugInfo.json)
USD is built from libraries that register their types/schemas/file-formats **at runtime** via a plugin registry. `PlugRegistry` discovers plugins by scanning **`plugInfo.json`** manifests that ship in a `usd/` resource tree ([API: PlugRegistry](https://openusd.org/release/api/class_plug_registry.html)). The lowest layer, **Tf** ("Tools Foundation"), provides core utilities including the error/diagnostic system (`TfError`) and the static-registration macros (`TF_REGISTRY_FUNCTION`) ([Tf overview](https://openusd.org/release/api/tf_page_front.html)).

**Why it matters here — this is the spec's "landmine" (task 0.5).** Even a monolithic USD build still needs that `usd/` resource tree *discoverable at runtime*, or nothing works and the failure can be silent. We ship `usd/` next to the binaries and call `PlugRegistry::RegisterPlugins()` explicitly at extension init, with a path resolved **relative to the extension DLL** (not the working directory, which is unpredictable inside the editor). M0 has an integration test that *fails loudly* if core plugins aren't registered. `TfError` marks are also what we translate into Godot error macros (task 1.9).
→ see also [Static initializers & the monolithic build](#static-initializers--the-monolithic-build), [Error handling pattern](#error-handling-tferror--godot-macros)

### oneTBB
**oneAPI Threading Building Blocks** — a C++ library for parallelism (parallel loops, task scheduling, concurrent containers). After Boost was removed (24.11), oneTBB is OpenUSD's **one remaining hard third-party dependency** ([repo, UXL Foundation](https://github.com/uxlfoundation/oneTBB), [docs](https://uxlfoundation.github.io/oneTBB/)). The runtime DLL is `tbb12.dll`.

**Why it matters here:** It must ship alongside our binaries (declared in the `.gdextension` `[dependencies]`), and — like everything else — it must be built against the **dynamic CRT (`/MD`)** so it doesn't fight `usd_ms.dll` over the heap. OpenUSD 26.05 pins **oneTBB 2021.9**, built via `build_usd.py --onetbb`; without `--onetbb` the script builds the legacy Intel TBB 2020.3, which produces `tbb.dll` instead of the `tbb12.dll` we ship.
→ see also [The C runtime: /MD vs /MT](#the-c-runtime-md-vs-mt-the-heap-corruption-landmine)

### Error handling: TfError → Godot macros
USD reports recoverable problems through **Tf diagnostics** — error marks and posted errors rather than (mostly) exceptions ([Tf diagnostics](https://openusd.org/release/api/page_tf__diagnostic.html)). You scope a `TfErrorMark`, run USD calls, then inspect whether errors accumulated.

**Why it matters here:** Task 1.9. Malformed files must *fail gracefully and never crash the editor*. We drain Tf errors at translation boundaries and surface them through Godot's `ERR_*`/`WARN_*` macros so a bad asset produces a red console line, not a hard crash of the user's editor session.

---

## 2. Godot & GDExtension — the target engine

### Godot Engine 4.x
A free, open-source (MIT-licensed) cross-platform 2D/3D game engine, written in C++ ([godotengine.org](https://godotengine.org/), [repo](https://github.com/godotengine/godot)). **4.6** is the pinned target — it shipped in late January 2026 and the latest patch at decision time is **4.6.3** (May 2026) ([4.6 release notes](https://godotengine.org/releases/4.6/), [4.6.3 tag](https://github.com/godotengine/godot/releases/tag/4.6.3-stable)). Notable 4.6 items: Jolt physics is the default 3D engine, a new "Modern" editor theme, and rearrangeable docks. 4.7 is in development snapshots.

**Why it matters here:** Godot is the host. We pin **4.6** because the pin sets the compatibility *floor*, not a ceiling, and the editor-importer spike (task 3.1) wants the newest GDExtension API surface (**OQ-1, resolved**). Patch releases don't change the extension API, so we track `4.6.x` freely but the CI smoke test always downloads the *exact* pinned patch, never "latest."
→ see also [GDExtension](#gdextension), [The import pipeline](#the-import-pipeline-editorsceneformatimporter-vs-editorimportplugin)

### GDExtension
Godot's mechanism for letting the engine load **native shared libraries (DLL/.so/.dylib) at runtime** and have them register new classes, nodes, and resources — *without recompiling the engine* ([what is GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html)). It's a C ABI; the C++ ergonomics come from [godot-cpp](#godot-cpp). A small **`.gdextension`** text file tells the editor where the library is and which Godot/version it targets.

**Why it matters here:** It's how we add USD import *as an addon* rather than forking the engine — the whole point of "shipped, community-installable." A GDExtension built for 4.6 runs on 4.6+ but not earlier ([forward-compat note](https://github.com/godotengine/godot-cpp)).
→ see also [The .gdextension file & [dependencies]](#the-gdextension-file--dependencies), [godot-cpp](#godot-cpp)

### godot-cpp
The official **C++ bindings** for GDExtension — wraps the C API with real Godot classes (`Node3D`, `Ref<>`, `ClassDB` registration, etc.) ([repo](https://github.com/godotengine/godot-cpp), [C++ example](https://docs.godotengine.org/en/stable/tutorials/scripting/cpp/gdextension_cpp_example.html)). Consumed as a **git submodule** built alongside your extension. With no per-minor 4.6 branch/tag yet, the current line is **v10 (`master`)**, which targets Godot 4.3+ (incl. 4.6) via an `api_version` parameter.

**Why it matters here:** It's our submodule, pinned to a **v10/`master`** commit and targeting Godot 4.6 via `api_version` (task 0.1) — godot-cpp has no `godot-4.6-stable` branch/tag; re-pin to the v10 stable release branch once it ships. Two non-default build settings are load-bearing: godot-cpp defaults to **exceptions OFF and static CRT** on Windows, but USD needs **exceptions ON, RTTI ON, and dynamic CRT (`/MD`)** — so we override godot-cpp's defaults to match. Mismatching these is a primary source of link errors and heap corruption.
→ see also [C++ — exceptions & RTTI](#c--exceptions--rtti), [The C runtime: /MD vs /MT](#the-c-runtime-md-vs-mt-the-heap-corruption-landmine), [Git submodules](#git-submodules)

### The scene tree & core nodes (Node3D, MeshInstance3D, ArrayMesh, PackedScene)
Godot scenes are trees of **Nodes**. The ones we produce:
- **`Node3D`** — base node with a 3D `Transform3D`; our target for USD `Xform`/`Scope` ([class](https://docs.godotengine.org/en/stable/classes/class_node3d.html)).
- **`MeshInstance3D`** — a node that draws a `Mesh` resource in the scene ([class](https://docs.godotengine.org/en/stable/classes/class_meshinstance3d.html)).
- **`ArrayMesh`** — a `Mesh` built from raw vertex/normal/UV/index arrays, **one material per *surface*** ([class](https://docs.godotengine.org/en/stable/classes/class_arraymesh.html)). This is the data target for translated USD meshes.
- **`PackedScene`** — a serialized, reusable scene subtree (a `.tscn`/`.scn`); what an *editor import* ultimately produces ([class](https://docs.godotengine.org/en/stable/classes/class_packedscene.html)).
- **`Transform3D`** — Godot's transform type: a 3×3 `basis` (rotation/scale) plus an `origin` vector, **column-major** ([class](https://docs.godotengine.org/en/stable/classes/class_transform3d.html)).

**Why it matters here:** These are the right-hand side of every translation rule. The runtime loader (v0.1) returns a live `Node3D` tree; the editor importer (v0.3) wraps the same core to emit a `PackedScene`. "One material per surface" is exactly why [GeomSubsets](#geomsubsets) become multiple surfaces.
→ see also [Xforms & transforms](#xforms--transforms), [Vertices, normals, UVs & buffers](#vertices-normals-uvs--vertexindex-buffers)

### The import pipeline: EditorSceneFormatImporter vs EditorImportPlugin
Godot's editor turns source files in the **FileSystem dock** into engine resources on import (writing a sidecar `.import` file). Two extension points matter:
- **`EditorSceneFormatImporter`** — the **3D-scene pathway** that glTF/FBX use; it plugs into the standard 3D import dock (with its scene options) and yields a scene ([class](https://docs.godotengine.org/en/stable/classes/class_editorsceneformatimporter.html); the glTF impl is [`EditorSceneFormatImporterGLTF`](https://docs.godotengine.org/en/stable/classes/class_editorsceneformatimportergltf.html)).
- **`EditorImportPlugin`** — the **general resource importer**; you define recognized extensions, options, and the import step, and register it via `EditorPlugin.add_import_plugin` ([class](https://docs.godotengine.org/en/stable/classes/class_editorimportplugin.html)).

**Why it matters here:** v0.3's differentiation claim is "imports like glTF," so `EditorSceneFormatImporter` is *preferred* — it inherits the familiar 3D import UX for free. The risk is whether its virtual methods are overridable from a **GDExtension** (vs only from engine C++) on Godot 4.6; **task 3.1 is a spike to confirm exactly that**, with `EditorImportPlugin` as the proven fallback that still ships a working v0.3.
→ see also [GDExtension](#gdextension)

### GDScript
Godot's built-in, Python-flavored scripting language ([docs](https://docs.godotengine.org/en/stable/tutorials/scripting/gdscript/index.html)). Classes registered by a GDExtension become callable from GDScript like any engine class.

**Why it matters here:** The v0.1 runtime API is GDScript-facing: `UsdStageLoader.load(path) -> Node3D`. It's the fastest way to demo and test the translation core with zero editor plumbing, and our GUT tests are written in GDScript against it.
→ see also [GUT](#gut-godot-unit-test)

### The `.gdextension` file & `[dependencies]`
The text manifest that registers the extension with Godot. It declares `compatibility_minimum`, the per-platform library paths, and — critically — a **`[dependencies]`** section listing the *other* DLLs that must sit beside the extension and be deployed with it ([the .gdextension file](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/gdextension_file.html)).

```ini
[configuration]
entry_symbol = "godot_usd_bridge_init"
compatibility_minimum = 4.6

[libraries]
windows.release.x86_64 = "res://addons/godot-usd-bridge/libgodot-usd.windows.release.x86_64.dll"

[dependencies]
windows.release.x86_64 = { "usd_ms.dll" : "", "tbb12.dll" : "" }
```

**Why it matters here:** This is the *deployment* half of the "works on my machine" mitigation. Declaring `usd_ms.dll` and `tbb12.dll` here is what makes a clean project pick them up. CI installs the packaged addon zip into a *fresh* project and runs the smoke test to prove the declaration is right.
→ see also [The plugin system](#the-plugin-system-tf-plug-plugregistry-pluginfojson), [Static vs dynamic linking & DLLs](#static-vs-dynamic-linking--dlls)

### Headless mode (`godot --headless`)
Runs the engine/editor with no window or rendering device — for servers, automation, and CI ([command-line tutorial](https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html)).

**Why it matters here:** Our GUT suite runs under `godot --headless` in GitHub Actions (task 0.7). The CI job downloads the *exact* pinned 4.6.x binary, loads the addon, and runs goldens — no GPU needed.

### GUT (Godot Unit Test)
The de-facto **in-engine unit-testing framework** for Godot, by *bitwes* — you write tests in GDScript, with a rich assert library, doubling/mocking, a CLI, and JUnit-XML output ([repo](https://github.com/bitwes/Gut), [docs](https://gut.readthedocs.io/)). The 9.x line targets Godot 4 and has a 4.6-compatible release; MIT-licensed.

**Why it matters here:** Our chosen in-engine test harness (`GUT + golden .usda fixtures`). It runs headless in CI and asserts the *exact* vertex/index counts and AABBs from the [vertex-split](#primvars--interpolation--and-vertex-splitting-the-heart) work. **OQ-3 was resolved (July 2026)**: [doctest](#doctest) joins it for C++-level unit tests of `translate/`'s pure math — GUT covers the in-engine, end-to-end layer; doctest covers the math beneath it.

---

## 3. C++ build & toolchain — the systems layer

### C++ — exceptions & RTTI
Two standard C++ features that compilers can toggle. **Exceptions** are the `throw`/`try`/`catch` error-propagation mechanism ([cppreference](https://en.cppreference.com/w/cpp/language/exceptions)). **RTTI** (Run-Time Type Information) backs `dynamic_cast` and `typeid` — runtime type queries on polymorphic objects ([cppreference: typeid](https://en.cppreference.com/w/cpp/language/typeid)). Both add a little binary/runtime overhead, so engines often disable them — Godot/godot-cpp ship with exceptions off by default.

**Why it matters here:** OpenUSD **requires both ON**. The catch is that *the whole module must agree* — you can't link exception-using USD into an exceptions-off extension and expect correct unwinding. So our CMake presets force `/EHsc` (exceptions) and RTTI on, overriding godot-cpp's defaults (task 0.2). This is a *claimed skill* (Tier-2 with full comprehension required) — the *why* needs to be fully understood, not just the flag values.
→ see also [godot-cpp](#godot-cpp), [CMake & CMakePresets](#cmake--cmakepresets)

### MSVC / Visual Studio 2022
**MSVC** is Microsoft's C++ compiler (`cl.exe`) and toolchain; **Visual Studio 2022** is the IDE/build environment that ships it ([MSVC docs](https://learn.microsoft.com/en-us/cpp/)). The pinned toolchain for this Windows-first project.

**Why it matters here:** Windows is the primary platform, so MSVC's conventions (CRT model, name mangling, `__declspec(dllexport)`) define the build. CMake drives MSVC; we never hand-invoke `cl` with ad-hoc flags (the CLAUDE.md rule), because the load-bearing flags must stay pinned and reproducible.
→ see also [The C runtime: /MD vs /MT](#the-c-runtime-md-vs-mt-the-heap-corruption-landmine)

### The C runtime: /MD vs /MT *(the heap-corruption landmine)*
MSVC's **C Runtime (CRT)** can be linked two ways. **`/MD`** uses the **dynamic** CRT — your module links a small import lib (`MSVCRT.lib`) and the real code lives in shared DLLs (`vcruntime`/`ucrt`/`msvcp`) loaded at runtime. **`/MT`** statically links the entire CRT *into each module* ([official `/MD /MT` docs](https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library)). The crucial consequence: **the CRT holds global state, including the heap.** With `/MD`, an EXE and all its DLLs share *one* CRT — so memory allocated in one module can be freed in another. With `/MT`, each module has its *own* private heap and globals; pass a pointer across a module boundary and free it on the other side and you get **corruption or crashes**. Microsoft's own guidance is to never mix static and dynamic CRTs in one process.

**Why it matters here — top-tier project landmine.** `usd_ms.dll` and `tbb12.dll` use the dynamic CRT. godot-cpp **defaults to the static CRT on Windows**. If we leave that default, our extension and USD end up with *different heaps* and objects crossing the boundary corrupt memory — an intermittent, maddening bug. So the spec mandates **`/MD` everywhere**, pinned in the CMake presets and documented in `building.md`. Risk-table entry: "CRT / exceptions / RTTI mismatch causing heap corruption or link errors."
→ see also [godot-cpp](#godot-cpp), [Static vs dynamic linking & DLLs](#static-vs-dynamic-linking--dlls), [oneTBB](#onetbb)

### Static vs dynamic linking & DLLs
**Static linking** copies a library's code into the consuming binary at build time (one bigger, self-contained file). **Dynamic linking** keeps the library as a separate **DLL** loaded at runtime; the consumer links a small **import library** (`.lib`) that resolves the symbols ([Microsoft: DLLs](https://learn.microsoft.com/en-us/cpp/build/dlls-in-visual-cpp), [linking](https://learn.microsoft.com/en-us/cpp/build/reference/linking)). DLLs let multiple modules share one copy and let you ship updates without relinking everyone.

**Why it matters here:** Our packaging model is **monolithic *shared*** — ship `usd_ms.dll` + `tbb12.dll` next to `libgodot-usd.dll`. We deliberately *defer* static-linking USD into the extension (see next entry). Understanding import libs vs the runtime DLL is also why `/MD` produces a dependency on the CRT DLLs that must be present (typically via the VC++ Redistributable or shipped locally).
→ see also [The .gdextension file & [dependencies]](#the-gdextension-file--dependencies)

### Static initializers & the monolithic build
A **static initializer** is code that runs *before* `main()` (or on DLL load) to set up global/namespace-scope objects. USD's plugin/type registration leans on these via the `TF_REGISTRY_FUNCTION` machinery. The hazard: when you **static-link** a library, the linker can **strip "unreferenced" objects** — and registration objects look unreferenced because nothing calls them directly; they work purely by their constructor side effects. Strip them and the registrations silently never happen. A **monolithic build** (`PXR_BUILD_MONOLITHIC=ON`) collapses USD's many libraries into a single shared library (`usd_ms`), which keeps the registration machinery intact on the well-trodden shared path ([USD building options](https://github.com/PixarAnimationStudios/OpenUSD/blob/release/BUILDING.md)).

**Why it matters here:** This is *the* reason the spec chooses **monolithic shared** over static-into-the-extension. Static linking USD risks stripping `TF_REGISTRY` initializers (registrations vanish, nothing loads). It's also why the [plugInfo](#the-plugin-system-tf-plug-plugregistry-pluginfojson) tree still has to ship and be explicitly registered.
→ see also [The plugin system](#the-plugin-system-tf-plug-plugregistry-pluginfojson)

### CMake & CMakePresets
**CMake** is the cross-platform **build-system generator**: you describe targets/dependencies in `CMakeLists.txt`, and it emits native build files (here, MSVC/Visual Studio projects) ([docs](https://cmake.org/cmake/help/latest/)). **CMakePresets** (`CMakePresets.json`) capture named configure/build presets — flags, generators, cache vars — so a build is one command and identical everywhere ([cmake-presets manual](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)).

```jsonc
{
  "version": 6,
  "configurePresets": [{
    "name": "windows-release",
    "generator": "Visual Studio 17 2022",
    "cacheVariables": {
      "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreadedDLL",   // = /MD
      "CMAKE_CXX_FLAGS": "/EHsc /GR"                       // exceptions + RTTI
    }
  }]
}
```

**Why it matters here:** USD is CMake-native, so CMake won (SCons dropped). Presets are how the **load-bearing flags** (`/MD`, exceptions, RTTI) stay pinned and un-hand-editable (tasks 0.2; CLAUDE.md "CMake presets only"). The preset *drafting* is Tier-2 but the flags demand full comprehension.
→ see also [The C runtime: /MD vs /MT](#the-c-runtime-md-vs-mt-the-heap-corruption-landmine), [C++ — exceptions & RTTI](#c--exceptions--rtti)

### doctest
A **single-header C++ testing framework** ([repo](https://github.com/doctest/doctest), [tutorial](https://github.com/doctest/doctest/blob/master/doc/markdown/tutorial.md)) in a specific niche: Catch2-style ergonomics at the lowest compile-time cost of the feature-rich frameworks. Tests self-register via `TEST_CASE` macros; plain assertions (`CHECK` continues on failure, `REQUIRE` aborts the case) *decompose expressions* so failures print both operand values; `SUBCASE` gives shared-setup branching (the body re-runs from the top per subcase path); `doctest::Approx` handles floating-point comparison. With `DOCTEST_CONFIG_IMPLEMENT` the host supplies its own `main()`, initializing libraries before tests run. The name honors Python's doctest — tests *can* co-locate with production code and compile out via `DOCTEST_CONFIG_DISABLE` (a capability this project deliberately doesn't use: Tier-1 files stay clean, the extension DLL stays lean).

**Why it matters here:** Resolves **OQ-3** (July 2026, at the v0.1 kickoff): `src/translate/` gets C++-level unit tests *alongside* the GUT + goldens baseline, because the conventions module (task 1.1) was designed as pure math — `(metersPerUnit, upAxis) → Transform3D` — testable with no stage, no editor, no engine (godot-cpp's math types are engine-independent). Pinned as a submodule at `extern/doctest`, same pattern as godot-cpp. The custom-`main` hook is load-bearing: the test executable must call `PlugRegistry::RegisterPlugins` before any stage-reading test, or USD fatally asserts (the same failure CI's negative test guards).
→ see also [GUT (Godot Unit Test)](#gut-godot-unit-test), [CMake & CMakePresets](#cmake--cmakepresets), [The plugin system](#the-plugin-system-tf-plug-plugregistry-pluginfojson)

### Boost (historical)
**Boost** is a large collection of peer-reviewed C++ libraries that historically were a heavyweight dependency of pre-24.x USD on Windows ([boost.org](https://www.boost.org/)). OpenUSD has been **Boost-free since the 24.11 release**.

**Why it matters here:** It's the reason a Docker build *used to* make sense and now doesn't — the old Boost-laden Windows build was painful enough to containerize. With Boost gone, the 26.x core-only build is small, which is the premise of **ADR-002** (no Docker).
→ see also [Docker / containerized builds](#docker--containerized-builds)

---

## 4. Version control, CI/CD & project hygiene

### Git
The distributed version-control system tracking the project's history ([git-scm.com](https://git-scm.com/), [Pro Git book](https://git-scm.com/book/en/v2)).

**Why it matters here:** Beyond the obvious, two Git features are *load-bearing* in this project's design: **submodules** (pinning godot-cpp) and **trailers** (the per-commit AI-Assist record). Both below.

### Git submodules
A **submodule** embeds another Git repo inside yours at a **pinned commit** — your repo records *which exact commit* of the dependency to use, and updates are deliberate ([Pro Git: submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules)).

```bash
git submodule add https://github.com/godotengine/godot-cpp extern/godot-cpp
cd extern/godot-cpp && git checkout master && cd -   # v10: targets 4.3+ incl. 4.6 via api_version
git add extern/godot-cpp && git commit -m "Add godot-cpp submodule (v10/master)"
# fresh clone:
git clone --recursive <repo>      # or: git submodule update --init --recursive
```

**Why it matters here:** godot-cpp is consumed as a submodule pinned to a **v10/`master`** commit (task 0.1) — there's no `godot-4.6-stable` branch/tag, so v10 targets Godot 4.6 via `api_version`. The pinned commit is what makes builds reproducible and keeps the ABI matched to the target Godot; re-pin to the v10 stable release branch once it's published. Forgetting `--recursive` on clone is a common first-build failure worth documenting in `building.md`.
→ see also [godot-cpp](#godot-cpp)

### Git trailers & `interpret-trailers`
**Trailers** are structured `Key: value` lines at the *end* of a commit message (the same shape as `Signed-off-by:`); Git can parse and add them mechanically with `git interpret-trailers`, and you can query them with `git log --format` ([git-interpret-trailers](https://git-scm.com/docs/git-interpret-trailers), [git-log](https://git-scm.com/docs/git-log)).

```
Implement faceVarying vertex splitting for st + normals

...body...

AI-Assist: tier-1
```
```bash
# share of tier-1 commits touching the translation core:
git log --format='%(trailers:key=AI-Assist,valueonly)' -- src/translate/ | sort | uniq -c
```

**Why it matters here:** This is the mechanism behind **AQ-2 (resolved)** in the AI policy — every commit records `AI-Assist: tier-1 | tier-2 | tier-3 | none`. Because trailers are machine-readable, the milestone self-audits (Rule 7) can report *real numbers* (e.g. "X% of `src/translate/` commits are tier-1"), which is far more defensible than a vibe.
→ see also the AI policy `AQ-2`

### Semantic versioning & tagged releases
**SemVer** is the `MAJOR.MINOR.PATCH` convention: breaking / feature / fix ([semver.org](https://semver.org/)). A Git **tag** marks a specific commit as a release; on GitHub a tag can carry a **Release** with attached build artifacts ([GitHub releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)).

**Why it matters here:** "Ship tagged releases with prebuilt Windows binaries" is a stated project goal. Our milestones map to versions (M0 → v0.1 → v0.2 → **v0.3.0 tagged release** with the addon zip). The release *is* the deliverable.
→ see also [CI/CD & GitHub Actions](#cicd--github-actions)

### CI/CD & GitHub Actions
**Continuous Integration** = every change is automatically built and tested on a clean machine, so breakage surfaces immediately ([Fowler on CI](https://martinfowler.com/articles/continuousIntegration.html)); **Continuous Delivery** extends that to producing release-ready artifacts ([Fowler on CD](https://martinfowler.com/bliki/ContinuousDelivery.html)). **GitHub Actions** is GitHub's built-in automation that runs these workflows in YAML ([docs](https://docs.github.com/en/actions)), with **caching** to avoid rebuilding expensive dependencies ([caching deps](https://docs.github.com/en/actions/using-workflows/caching-dependencies-to-speed-up-workflows)).

**Why it matters here — "CI from day one."** The USD build is expensive, so it runs **once per pin-bump and is cached** as an artifact; routine CI just builds the extension and runs the headless GUT smoke test on the exact pinned Godot. CI is also our reproducibility layer (the ADR-002 substitute for Docker) and the "clean-project install" guard against DLL-deployment bugs. `windows-latest` is primary; `ubuntu-latest` joins in v0.2+ for CI sanity. The CI YAML itself is Tier-3 (delegated), behavior sanity-checked not line-audited.
→ see also [Docker / containerized builds](#docker--containerized-builds), [Headless mode](#headless-mode-godot---headless)

### Docker / containerized builds
**Docker** packages software and its dependencies into portable **containers** for reproducible environments ([docs](https://docs.docker.com/), [get started](https://docs.docker.com/get-started/)). On Windows, native builds need **Windows containers** plus VS Build Tools ([Windows containers](https://learn.microsoft.com/en-us/virtualization/windowscontainers/about/)), which are heavier than the Linux norm.

**Why it matters here — we deliberately *don't* use it (ADR-002).** A containerized Windows build was the traditional way to tame pre-24.x USD's Boost sprawl. With Boost gone and a core-only 26.x build, a `scripts/build_usd.ps1` + cached GitHub Actions covers reproducibility without container overhead. Docker (Windows containers + VS Build Tools) stays a *documented fallback* if local/CI drift becomes a real problem — a decision, not an omission.
→ see also [Boost (historical)](#boost-historical), [ADRs](#adrs-architecture-decision-records)

### ADRs (Architecture Decision Records)
A lightweight, append-only log of **significant decisions** — each a short doc capturing context, the decision, and consequences, so future-you knows *why*, not just *what* ([Nygard's original post](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions), [adr.github.io](https://adr.github.io/), [templates collection](https://github.com/joelparkerhenderson/architecture-decision-record)).

**Why it matters here:** The project keeps `docs/adr/` — **ADR-001** (OpenUSD over `tinyusdz`), **ADR-002** (no Docker), and more as they arise. Under the AI policy, *the maintainer writes the ADRs* ("if I can't write the why, I don't own the change"). ADRs are the durable answer to "why did you build it this way?"

### MIT License (+ third-party license redistribution)
A short, permissive open-source license: use/modify/redistribute freely, keep the copyright notice, no warranty ([OSI text](https://opensource.org/license/mit)). Godot, godot-cpp, GUT, and OpenUSD are all MIT/Apache-style permissive.

**Why it matters here:** The repo is MIT (task 0.1). Because we **redistribute binaries** (`usd_ms.dll`, `tbb12.dll`), we must also ship the **upstream license texts** (OpenUSD's, etc.) in the addon — a real obligation, not boilerplate. Permissive upstream licenses are part of why this project is shippable at all. Specifics: OpenUSD ships a **modified Apache 2.0** `LICENSE.txt` (copy it verbatim — a generic Apache 2.0 is not a valid substitute) plus a `NOTICE.txt`; oneTBB is standard Apache 2.0; Apache 2.0 §4(d) requires carrying any upstream `NOTICE` into redistributions. These live in `addons/godot-usd-bridge/THIRD_PARTY_LICENSES/{OpenUSD,oneTBB}/`.

---

## 5. Graphics & data fundamentals — cross-cutting

### Coordinate systems & handedness
3D conventions differ by tool: which axis points **up** (Y-up vs Z-up) and the **handedness** (right- vs left-handed) of the coordinate frame ([handedness overview](https://en.wikipedia.org/wiki/Right-hand_rule)). USD defaults to **Y-up, right-handed**; many DCCs (Houdini, some Blender exports) author **Z-up**. Godot is **Y-up, right-handed** with **-Z forward**.

**Why it matters here:** The conventions module (task 1.1) builds the **root correction transform** that reconciles the stage's `upAxis`/units with Godot's frame. Z-up content imported without correction lands on its side. This is foundational Tier-1 math that everything else sits on top of.
→ see also [Xforms & transforms](#xforms--transforms), [Conventions](#conventions-metersperunit-upaxis-orientation-purpose-visibility)

### Winding order & face culling
A triangle's **winding** — the order its vertices are listed, **CCW** (counter-clockwise) or **CW** — determines which side is the "front" face, and the renderer **culls** (skips drawing) back faces for performance ([face culling](https://en.wikipedia.org/wiki/Back-face_culling)). USD's default mesh `orientation` is **`rightHanded` (CCW)**; Godot treats **clockwise** as front-facing (matching its glTF importer's handling).

**Why it matters here:** Task 1.5. We **reverse the triangle index order** (CCW→CW) so faces aren't inside-out in Godot, and we honor per-mesh `orientation = leftHanded` (which inverts the rule). Get it wrong and models look hollow/inverted or are invisible from the "wrong" side. `doubleSided` USD meshes map to disabling culling (task 2.4).
→ see also [Polygon triangulation](#polygon-triangulation-fan--ear-clipping)

### Polygon triangulation (fan & ear clipping)
GPUs draw triangles, but USD meshes contain **n-gons** (faces with >3 vertices). **Triangulation** splits each face into triangles. **Fan triangulation** picks one anchor vertex and connects it to every edge — trivial and correct *only for convex* polygons ([fan triangulation](https://en.wikipedia.org/wiki/Fan_triangulation)). **Ear clipping** handles general (concave, simple) polygons correctly at more cost ([polygon triangulation](https://en.wikipedia.org/wiki/Polygon_triangulation)).

**Why it matters here:** Task 1.3 (Tier-1). The spec starts with **fan triangulation** and *documents the convex assumption*, deferring ear-clipping until real assets demand it. Owning this means being able to say exactly when the fan approach breaks (concave faces) and what the upgrade path is.
→ see also [Meshes (UsdGeomMesh)](#meshes-usdgeommesh)

### Vertices, normals, UVs & vertex/index buffers
A GPU mesh is a **vertex buffer** (each vertex = position + attributes like **normal**, **UV/`st`** texture coordinates, color) plus an **index buffer** (triangles as triples of vertex indices). The defining constraint: **one attribute set per vertex** — a vertex referenced by many triangles contributes the *same* normal and UV to all of them ([glTF mesh data model](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes)). Godot's `ArrayMesh` is exactly this shape.

**Why it matters here:** This single-attribute-per-vertex rule is *why* [faceVarying primvars force vertex splitting](#primvars--interpolation--and-vertex-splitting-the-heart). USD can store per-face-corner UVs/normals; the GPU can't, so shared points must be duplicated into distinct vertices. Internalizing the buffer model makes the vertex-split algorithm obvious rather than mysterious.
→ see also [Primvars & interpolation](#primvars--interpolation--and-vertex-splitting-the-heart)

### sRGB vs linear color
Color textures are usually stored **sRGB-encoded** (gamma-corrected, perceptually weighted) because that's how they were authored and viewed, but lighting math must happen in **linear** space ([sRGB](https://en.wikipedia.org/wiki/SRGB), [linear workflow primer](https://docs.godotengine.org/en/stable/tutorials/3d/standard_material_3d.html)). So: **albedo/emissive** are sRGB and must be flagged for decode-on-sample; **normal, roughness, metallic** carry *data*, not color, and must be treated as **linear** (decoding them would corrupt the values).

**Why it matters here:** Task 2.2, and the sRGB/linear *decision* is explicitly **Tier-1** (the file I/O around it is Tier-2). Tagging a normal map as sRGB, or an albedo as linear, is a subtle, common bug that makes materials look washed-out or wrong. We set the correct color space per channel when building Godot textures from `UsdUVTexture`.
→ see also [Materials](#materials-usdshade-usdpreviewsurface-usduvtexture)

### glTF & FBX (the interchange formats Godot already imports)
**glTF 2.0** is Khronos's open, modern runtime 3D-asset format — Godot's first-class import path ([Khronos glTF](https://www.khronos.org/gltf/), [spec](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)). **FBX** is Autodesk's widely-used (proprietary) format, which Godot imports via a glTF-conversion route.

**Why it matters here:** Two reasons. (1) **The UX target** — "drag a `.usd` in and it imports *like glTF*" means literally reusing the [`EditorSceneFormatImporter`](#the-import-pipeline-editorsceneformatimporter-vs-editorimportplugin) pathway glTF uses. (2) **The reference implementation** — Godot's glTF importer is our model for the hard bits (winding flip, unit handling, material mapping); when unsure how Godot "expects" something, look at how its glTF importer does it. Blender-exported glTF/FBX assets are also part of our acceptance corpus.
→ see also [The import pipeline](#the-import-pipeline-editorsceneformatimporter-vs-editorimportplugin), [Winding order & face culling](#winding-order--face-culling)

---

## 6. Quick reference: the pinned stack

| Component | Pin / choice | One-line why | Source |
|---|---|---|---|
| OpenUSD | **v26.05** (latest full release; `YY.MM` cadence, ~every 2–3 months) | Real composition semantics; Boost-free since 24.11 | [releases](https://github.com/PixarAnimationStudios/OpenUSD/releases) |
| Godot | **4.6** (test on latest 4.6.x; 4.6.3 at decision time) | Newest GDExtension surface; pin = floor not ceiling | [4.6 notes](https://godotengine.org/releases/4.6/) |
| godot-cpp | submodule @ **v10/`master`** commit (targets 4.6 via `api_version`) | ABI match; built with **exceptions + RTTI + /MD** (overrides defaults) | [repo](https://github.com/godotengine/godot-cpp) |
| Toolchain | **MSVC (VS 2022)**, **CMake ≥ 3.26** | Windows-first; USD is CMake-native | [MSVC](https://learn.microsoft.com/en-us/cpp/) · [CMake](https://cmake.org/cmake/help/latest/) |
| CRT | **`/MD` (dynamic) everywhere** | Match `usd_ms.dll`'s heap; avoid corruption | [/MD /MT](https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library) |
| USD build | **core-only, monolithic shared** (`usd_ms` + `tbb12`) | Small build; keeps `TF_REGISTRY` initializers intact | [BUILDING.md](https://github.com/PixarAnimationStudios/OpenUSD/blob/release/BUILDING.md) |
| Dependency | **oneTBB 2021.9** (`tbb12.dll`; built via `--onetbb`) | USD's one remaining hard third-party dep | [oneTBB](https://github.com/uxlfoundation/oneTBB) |
| CI | **GitHub Actions** (USD build cached; headless GUT) | Reproducibility layer; the ADR-002 Docker substitute | [Actions](https://docs.github.com/en/actions) |
| Tests | **GUT 9.x** + golden `.usda` fixtures | In-engine, headless, JUnit output; exact count asserts | [GUT](https://github.com/bitwes/Gut) |
| License | **MIT** (+ ship OpenUSD license for redistribution) | Permissive; legal to ship binaries | [MIT](https://opensource.org/license/mit) |

---

*Maintenance: this is a living doc. When a pin bumps or a new technology enters the project, add or update the entry **and** its upstream link so claims stay verifiable. Entries describing Tier-1 topics are background for understanding, not a license to paste implementation.*
