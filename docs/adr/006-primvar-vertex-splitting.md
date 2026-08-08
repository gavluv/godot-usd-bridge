# ADR-006: Primvar engine and faceVarying vertex splitting

- **Status:** Accepted
- **Date:** 2026-08-07

## Context

Godot's ArrayMesh has one index array addressing all attribute channels,
so a vertex is an indivisible tuple of (position, normal, uv, etc.). USD
has three simultaneous addressing domains: face, point and face-corner.
The corner is the finest domain any interpolation mode uses, so every
mode can be sampled at a corner. A vertex shared by adjacent faces with
differing corner values in another channel splits into new distinct
vertices. To convert a USD mesh to a Godot mesh, therefore, we sample
each corner into a tuple, dedup the tuples, then emit. The question that
follows is then: how are the tuples keyed for deduplication?

The original USD mesh's point indexing 0..P-1 can be preserved in the
output positions buffer by appending new split vertex positions starting at
P, reshaped to remove unreferenced vertices, or discarded.

## Decision

Key the tuple by *values* of present channels sampled at each corner
(position, normal, and uv as of task 1.4).

Initialize positions array directly from the USD mesh points, preserve
original point indices through deduplication, remove unreferenced vertices
and remap in a final pass.

## Alternatives considered

- **Key by address**
  > Under an address key, `constant`, `vertex`, and `varying` interpolation
  > modes are functional dependencies on the point index already present,
  > so only `uniform` and `faceVarying` contribute. Both put a component
  > in the key that is unique per corner, which means an address key yields
  > exactly one vertex per corner whenever any non-point-domain channel
  > exists. It cannot express partial splitting, and it inflates buffers
  > roughly 4x on closed quad meshes and 6x on closed triangle meshes when
  > an exporter writes `faceVarying` data that has no actual seams.

- **Preserve USD point ordering exactly**
  > Supported by debuggability and hand-computable goldens arguments.
  > Rejected because orphaned vertices cost buffer space and contribute to
  > AABB calculations used for culling in the pinned version of Godot.

- **Compact points by first-use-order**
  > Numbering is determined by traversal order, affected by triangulation
  > implementation. Expected output for goldens is no longer readable off
  > the fixture.

## Consequences

Keying by value necessitates float hashing; -0.0 vs +0.0, last-ULP, and
NaN comparison cause under-dedup with naive hashing. godot-cpp's
`HashMapComparatorDefault` specializations handle NaN equality and its
hasher normalizes NaN and -0.0/+0.0 before hashing. Substituting plain
`operator==` with `std::hash` reintroduces both problems; using godot-cpp's
comparator without its hasher yields keys that compare equal while hashing
differently, causing lookups to fail silently. Last-ULP differences remain
as an accepted cost of value keys; the result is extra vertices, not a
correctness issue. Keying on authored primvar indices instead of values is
output-identical if the value pool is pre-deduped, and is available if
profiling ever demands it.

Final vertex buffer no longer maps directly to the original mesh points;
vertex numbering is USD order among the surviving points. Compaction guards
against junk data in the result mesh and inflated culling bounds. For a
mesh with no unreferenced points the compaction pass is a no-op, so
predictability for tests is not lost.
