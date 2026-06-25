# ADR-001: Use OpenUSD over tinyusdz

- **Status:** Accepted
- **Date:** 2026-06-19

## Context

It is possible to read USD files without the official OpenUSD library, using
alternative file parsers. A minimal USD reader could trade features for
reduced build and dependency burden.

## Decision

We build against the real OpenUSD library rather than a minimal third-party
reader or a custom parser.

## Alternatives considered

- **tinyusdz** — dependency-free C++14 USD library.
  > Only supports a subset of USD features (e.g., experimental composition).

- **bespoke parser**
  > Greatly increases project scope.

## Consequences

Must build the real USD library and bundle it as a dependency for distribution.
Requires substantial toolchain and configuration overhead. Imposes build
settings requirements for godot-cpp (exceptions ON, RTTI ON, and /MD).

Real USD provides reference composition semantics, and we benefit from the
ability to use the full USD API for traversing and translating USD data.
