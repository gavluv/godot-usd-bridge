# ADR-004: Pin godot-cpp to v10/master with api_version

- **Status:** Accepted
- **Date:** 2026-06-23

## Context

godot-cpp's version branches stop at 4.5; there is no `godot-4.6-stable`
branch or tag yet. The unified v10 line (master) targets Godot 4.3+ and
selects the API via the `api_version` parameter.

## Decision

We pin godot-cpp to a specific v10/master commit and set `api_version = 4.6`,
rather than tracking a Godot-version branch or tag.

## Alternatives considered

- **A `godot-4.6-stable` branch/tag** — the natural pin, but it does not exist at
  decision time.
  > The godot-cpp repo README states, "Starting with version 10.x, godot-cpp is
  > versioned independently from Godot. Using the `api_version` parameter [...],
  > godot-cpp v10 can target Godot 4.3 or later (including 4.6)." This implies
  > that a `godot-4.6-stable` branch/tag may never exist.

## Consequences

The godot-cpp README states that version 10.x is currently in Beta; re-pin from
master to the v10 stable release branch once it lands. Verify the exact
`api_version` form at CMake setup.
