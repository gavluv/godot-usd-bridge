# CLAUDE.md — godot-usd-bridge

## Working agreement
- Code in `src/translate/` is hand-written by the maintainer (Tier 1). In that
  directory: review, explain, suggest test cases, and critique — but do
  NOT write or modify implementation code unless explicitly asked to.
- For everything else: propose a plan and wait for approval before
  writing code, unless told the task is delegated (Tier 3).
- When I'm debugging Tier 1 code, act as a sounding board: ask questions,
  challenge hypotheses, point at relevant docs. Do not write the fix.
- Always cite which OpenUSD/godot-cpp header or doc an API claim comes
  from; flag uncertainty instead of guessing.
- Keep diffs small and single-purpose.

## Project context
- Spec: docs/godot-usd-bridge-spec.md (pinned versions, milestones, ADRs)
- Build: CMake presets only; never hand-invoke with ad-hoc flags
  (CRT /MD, exceptions ON, RTTI ON are load-bearing).
- Verify: run all four, the same commands CI runs, before calling a change
  done. `ctest` covers only the pure `src/translate/` files; only
  `check_godot.ps1` sees what the importer produces in Godot, such as log
  messages and imported node trees. Details in docs/building.md, "Testing".
  ```
  cmake --build --preset windows-debug
  ctest --preset windows-debug
  pwsh scripts/format.ps1 -Check
  pwsh scripts/check_godot.ps1
  ```
- Commits: Conventional Commits, each with an
  `AI-Assist: tier-1 | tier-2 | tier-3 | none` trailer
  (docs/godot-usd-bridge-ai-policy.md). The maintainer writes commit
  messages. A commit that keeps a higher tier while carrying AI-drafted parts
  names those parts in its body.

## windows.h macro collisions
USD headers pull in `windows.h`, whose macros collide with godot-cpp
identifiers. For example, `winnetwk.h` defines `CONNECT_DEFERRED`, which wrecks
the `ConnectFlags` enum in `godot_cpp/classes/object.hpp`, producing ~200 syntax
errors *inside a godot-cpp header* that name neither USD nor the file you edited.

`src/platform_fixup.h` neutralizes this: CMake force-includes it into every
translation unit, where it loads `windows.h` once and undefines the offenders.
Include order is therefore unconstrained anywhere in the project. If a new
collision appears, add an `#undef` to that header rather than reordering
includes.
