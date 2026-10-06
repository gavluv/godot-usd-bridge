# ADR-007: Convert to Godot mesh conventions in the pure C++ layer

- **Status:** Accepted
- **Date:** 2026-10-06

## Context

To translate a USD mesh to a Godot ArrayMesh, various convention conversions
must be performed, such as changing triangle winding order to clockwise when
necessary (USD's default rightHanded orientation is counter-clockwise), and
flipping the V-axis for UVs. These conversions could occur in the pure C++
"convert" layer, or the Godot engine-bridged "import" layer; only the pure
layer is built into the test executable (ADR-005).

## Decision

Convert to Godot conventions in the pure layer; MeshData holds per-mesh data
(winding, UV origin) already in Godot's conventions, and stage-level
conventions (units, up axis) stay a node transform.

## Alternatives considered

- **Convert to Godot conventions in the import layer**
  > Mesh data stays in USD conventions throughout the pure layer, so expected
  > triangles in existing doctests that assume USD conventions do not need to
  > be changed. Expected triangles arrays in fixture docs and golden needles
  > for the final imported Godot meshes must be updated when the winding flip
  > is applied.
  >
  > Rejected because mesh convention conversion is only testable within
  > Godot; doctest coverage is lost because the import layer is not built into
  > the test exe. Orientation still has to reach the import layer to perform
  > the winding flip conditionally; either the convert-side MeshData struct
  > must carry an orientation field, or the prim must be supplied and read
  > from a second time on the import side, splitting USD reads across two
  > layers. The UV V-flip is currently implemented in the pure layer, so
  > performing the winding flip in the import layer would similarly split
  > conventions across two layers.

## Consequences

Convention conversion is fully unit-testable in the test executable. MeshData
no longer mirrors the original USD file after conversions are performed. The
import layer remains a mechanical copy into Godot's arrays.

Expected triangles arrays that assert USD conventions, in fixture docs, golden
checks, and doctests using the `to_mesh_data` prim overload, must be updated
at the same time the winding flip is applied. Doctests using the `to_mesh_data`
array overload may still assert USD winding by passing `p_flip_winding=false`.

Future conversions added to the pure layer must also change every expected
value they touch in the same commit. A conversion that reorders triangle
indices, e.g., the winding flip, must run after the dedup walk, or it can
change the output mesh's vertex numbering (ADR-006).
