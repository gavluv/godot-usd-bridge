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
