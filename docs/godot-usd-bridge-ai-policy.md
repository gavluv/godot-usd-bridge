# Godot USD Bridge — AI-Assisted Development Policy

**Status:** Active · **Created:** June 2026 · Companion to `godot-usd-bridge-spec.md`

A living document. Revisit at the start of each milestone; elaborate as the workflow gets tested against reality.

---

## 1. Purpose & Goals

This project has two skill goals that are in tension, and this policy exists to resolve that tension deliberately rather than by drift:

1. **Build genuine C++ / engine-internals / USD depth.** The claim this project backs is "I can do this work," and that depth has to be real.
2. **Demonstrate effective AI-assisted engineering.** Harnessing AI well — knowing where it multiplies output and where it hollows out understanding — is itself a professional skill worth showcasing.

**The ownership standard:** ownership is not authorship of keystrokes; it is decision authority plus full comprehension. The test for every merged change: *could I defend this decision, explain this code, and debug it live, under questioning?* If yes, it's mine regardless of who typed it. If no, it doesn't merge.

**The failure mode being guarded against:** not AI involvement, but accumulating code whose decisions I can't reconstruct ("vibe coding").

---

## 2. The Three Tiers

Every artifact — code or prose — is assigned a tier *before work starts on it*, by who authored it and how deeply I reviewed. The rows below are the `AI-Assist` trailer values (AQ-2); **`none`** is the zero-AI baseline. Tier assignment is reviewed at each milestone kickoff.

| Tier | Who authors | AI's role | Rationale |
|---|---|---|---|
| **none — unassisted** | Me, alone | None at all — no tutoring, review, or drafting. | Genuinely AI-free artifacts (e.g., the GitHub license seed). |
| **1 — Learning target** | Me, by hand | Tutor before (explain concepts, quiz me); ruthless reviewer after (correctness, edge cases, allocation patterns; for prose: sourcing, accuracy, coherence). Never writes implementation. | This is the depth the project exists to build. |
| **2 — Understood plumbing** | AI drafts | I review line-by-line; must understand every line before accepting; rework freely. | Typing it teaches little, but comprehension is still mandatory — it's my codebase. |
| **3 — Commodity** | AI, delegated | I sanity-check behavior, not every line. | Pure commodity work; delegation buys time back for Tier 1. |

### Tier assignments (M0–v0.1)

| Tier 1 | Tier 2 | Tier 3 |
|---|---|---|
| All of `src/translate/`: conventions math (1.1), triangulation (1.3), **primvar engine / vertex splitting (1.4)**, winding (1.5), purpose & visibility (1.6), `ArrayMesh` assembly (1.7) | godot-cpp class registration boilerplate, `register_types.cpp` | `scripts/build_usd.ps1` (0.4) |
| `PlugRegistry` init & resource path resolution (0.5) | | CI workflow YAML (0.7) |
| Test fixture *selection and golden values* (1.8) — choosing what to test is understanding | CMake presets (0.2) — AI drafts, but the CRT/exceptions/RTTI flags are a claimed skill: full comprehension required | Repo scaffold, .gitignore, README *formatting* (0.1) |
| Debugging of any Tier 1 code | GUT test scaffolding/boilerplate (1.8), error-macro wiring (1.9) | `.usda` fixture file authoring (to my spec) |

These rows name *components*, but the `AI-Assist` trailer is per *commit* (AQ-2), so where a row is coarse the commit's actual content governs. "README formatting" is Tier-3, but **substantive README prose is Tier-2** — authored content I own, the same as the spec. A commit that spans tiers takes its **highest-touch tier**, or splits into provenance-homogeneous commits for literal honesty (the third-party licenses are split into their own `none` commit). When a commit keeps the higher tier while carrying AI-drafted parts, such as golden checks drafted alongside hand-written Tier 1 code, its body names those parts in prose. The trailer stays a single value, so trailer-based audits (AQ-2) still read it correctly.

`ArrayMesh` assembly (1.7) was listed as Tier 2 until the task 1.7 kickoff. It moved to Tier 1 because the code, `godot_mesh()`, lives in `src/translate/mesh_import.cpp`, and everything in that directory is hand-written (AQ-1).

### Provisional assignments (v0.2–v0.3, confirm at milestone kickoff)

- **Tier 1:** material resolution logic (2.1), GeomSubset → surface mapping (2.3), instancing strategy (3.4), variant enumeration design (3.3).
- **Tier 2:** texture I/O plumbing (2.2 file loading half; the sRGB/linear *decisions* are Tier 1), `StandardMaterial3D` property wiring (2.4), importer registration boilerplate (3.2).
- **Tier 3:** release packaging scripts (3.5), docs formatting.

---

## 3. Rules of Engagement

1. **Never accept a diff I couldn't have written myself, given time.** When that happens anyway, stop — study it until I could have, then merge. That gap is precisely the skill being bought.
2. **Plan before code (Tiers 1–2).** AI proposes an approach; I approve, push back, or redirect before implementation. I make design decisions; I don't ratify them.
3. **No "just fix it" loops.** Bouncing a build error or failing test to the AI repeatedly until green is where understanding silently leaks out. In Tier 1, I debug; AI is a sounding board ("here's what I observe, here's my hypothesis — poke holes"), not a fixer. In Tier 2, one round of AI fixing is fine *if* I can explain the fix afterward.
4. **I write all commit messages, ADRs, and devlogs.** If I can't write the "why," I don't own the change yet. That hand-authoring, even with AI tutoring or review, is what tags them **tier-1** — `none` is for artifacts AI never touched.
5. **Small diffs.** Review effort scales superlinearly with diff size; big AI diffs are where comprehension dies.
6. **Verify API claims against headers.** OpenUSD and Godot surfaces are large enough that hallucinated methods happen. Checking the actual header is also how the API gets into my head.
7. **Milestone self-audit.** At each milestone close, pick two merged changes at random and explain them out loud (rubber-duck or devlog draft). Failure on either → that area gets re-studied before the next milestone starts.
8. **Commit hygiene.** Commits follow Conventional Commits (format only; versioning stays manual and milestone-driven, not auto-derived). Each carries the `AI-Assist:` trailer (AQ-2). If PRs are ever adopted, **never squash-merge** — it collapses the per-commit trailers and defeats the milestone self-audit; use a merge or rebase-merge instead.

---

## 4. Enforcement via CLAUDE.md

The repo's [`CLAUDE.md`](../CLAUDE.md) encodes this policy as standing instructions, so discipline doesn't depend on in-the-moment willpower. Its working agreement covers the Tier 1 boundary around `src/translate/`, plan-before-code for everything else, sounding-board-only help when debugging Tier 1 code, citing a header for every API claim, and small diffs. It also carries the project context an agent needs in every session: build presets, the verification commands, commit conventions, and the `windows.h` macro fix. It was created in task 0.1 and has been refined since. It is linked here rather than copied, so the two can't drift; `CLAUDE.md` is the authoritative version.

---

## 5. Provenance & Disclosure

- **What the repo states plainly:** the translation core (`src/translate/`) and its doctests are hand-written, with Claude Code as reviewer and tutor (Tier 1), as are commit messages and ADRs (Rule 4). Claude Code drafts other artifacts for review: build and CI scripts, test fixtures and golden checks, and documentation prose such as the spec (Tiers 2 and 3). The repo rules above keep it from writing the learning-target code. The `AI-Assist:` trailer on each commit is the authoritative record (Section 6, AQ-2).
- **README:** a short "Development workflow" note linking to this policy.
- **Devlog:** one entry (task 3.6 series) dedicated to this workflow — what the tiers were, where the policy was hard to follow in practice, and what AI was genuinely good and bad at on a C++/USD codebase.

---

## 6. Open Questions

- **AQ-1:** Do Tier 2 components get promoted to Tier 1 if one proves more central than expected (e.g., the CMake/link engineering)? Default: yes, retroactively study to Tier 1 depth. First applied at the task 1.7 kickoff, to `ArrayMesh` assembly (Section 2).
- **AQ-2 — RESOLVED (June 2026):** Track per commit via a git trailer: `AI-Assist: tier-1 | tier-2 | tier-3 | none`, using the Section 2 definitions (all four values). No finer granularity (percentages are unmeasurable and gameable); the tier encodes what matters — decision authority and review depth. Adopted from the first maintainer-authored commit (the GitHub-generated license seed is excluded). Trailers are machine-readable (`git log --format` / `git interpret-trailers`), so milestone self-audits (Rule 7) can report real numbers, e.g. share of tier-1 commits touching `src/translate/`.
- **AQ-3:** Other tools (e.g., editor autocomplete/agents) — same tier rules apply, but inline autocomplete inside `src/translate/` may be too frictionless to police. Decide whether to disable it for that directory.