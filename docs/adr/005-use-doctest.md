# ADR-005: Use doctest for translation-core C++ unit tests

- **Status:** Accepted
- **Date:** 2026-07-13

## Context

We currently use GUT (Godot Unit Test) for in-engine tests, which requires
all testable methods be GDScript-visible. Testability gaps occur where C++
methods lack public API bindings. By shaping conventions as pure math over
godot-cpp's engine-independent types at v0.1 kickoff, C++-level testing
becomes nearly free. This resolves the spec's OQ-3.

## Decision

We adopt a C++ unit test layer for the translation core, using doctest,
alongside GUT. Pin doctest as a submodule, add a CTest target, and a custom
`main.cpp` for doctest to bootstrap USD before running tests.

## Alternatives considered

- **Testing exclusively with GUT**
  > Must have the Godot editor installed to run tests. Requires test
  > methods to be registered and callable from GDScript, either polluting
  > the API with test-only methods, or testing indirectly via goldens.
  > Cumbersome for non-Godot, pure C++ testing.

- **Other C++ testing frameworks, e.g., GoogleTest/Catch2**
  > More build surface and overhead; both are compiled libraries versus
  > doctest's single vendored header, and doctest's compile-time overhead
  > is the lowest of the feature-rich frameworks.

## Consequences

We introduce a second test layer alongside GUT, a new pinned submodule, and
a custom doctest bootstrapper to register USD plugins before running tests.
Test binary bakes absolute paths (plugin and fixture dirs), deliberately
non-relocatable, local/CI only. `TRANSLATE_SOURCES` (`src/translate`) compiles
into both the extension and test exe, requiring that any unit-tested translate
file be limited to touching only the engine-independent godot-cpp types.

GUT still owns engine-side testing, e.g., verifying the composed node
tree is correct, the GDScript API contract, the spec's golden/AABB
assertions.
