# ADR-002: No containerized build for OpenUSD

- **Status:** Accepted
- **Date:** 2026-06-22

## Context

Containerized builds were historically an attractive option for building OpenUSD
on Windows due to the dependency sprawl of older USD versions. A core-only build
of modern USD now only requires oneTBB as a hard dependency (no Boost).

## Decision

We do not use a containerized build for OpenUSD. The pinned build is driven by
`scripts/build_usd.ps1` plus CI caching.

## Alternatives considered

- **Containerized Windows build** (Windows containers + VS Build Tools).
  > Containerization adds overhead without much benefit over CI caching.

## Consequences

Avoids the maintenance overhead of Docker images and Windows container tooling.
GitHub Actions becomes the reproducible build workflow for USD; local build
environments are aligned by convention rather than by explicit capture.

Revisit containerization if local/CI drift becomes a real problem.
