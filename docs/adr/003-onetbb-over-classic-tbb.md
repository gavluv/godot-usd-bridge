# ADR-003: Build against oneTBB rather than classic Intel TBB

- **Status:** Accepted
- **Date:** 2026-06-23

## Context

OpenUSD requires building against either classic Intel TBB (2020.3) or oneTBB
(2021.9, USD's tested pin), producing `tbb.dll` or `tbb12.dll`, respectively.
The build_usd.py script defaults to Intel TBB; oneTBB is opt-in via `--onetbb`.
The addon ships its own TBB DLL; there is no host process whose TBB runtime has
to be matched.

## Decision

We build OpenUSD against oneTBB (2021.9) using `--onetbb`, and ship `tbb12.dll`.

## Alternatives considered

- **Classic Intel TBB 2020.3** (the build_usd.py default → `tbb.dll`).
  > Superseded by oneTBB, but still the default for legacy consumer
  > compatibility. Would be the necessary choice for in-process coexistence
  > with a host that already links Intel TBB, but that doesn't apply here.

## Consequences

Choosing oneTBB aligns us with the direction USD itself is moving. Building USD
requires passing `--onetbb` (currently opt-in, re-verify at the pinned tag).
