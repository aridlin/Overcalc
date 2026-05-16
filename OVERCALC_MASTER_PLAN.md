# OverCalc Master Plan

## North Star

**OverCalc is a terminal-native computational engine where beautiful math rendering is the default interface, not an add-on.**

Success means users can type rich LaTeX math in a terminal and get trustworthy, inspectable numeric/symbolic results with stunning output, fast feedback, and scriptable workflows.

---

## STAR Framework (Strategic)

### S — Situation
OverCalc aims to combine a CLI-first developer experience with high-quality terminal math rendering and long-term CAS capabilities. Existing tools usually trade off one of these:
- Strong rendering but weak terminal workflow,
- Strong CAS but weak terminal-native UX,
- Good CLI calculators but limited LaTeX-first interaction.

### T — Task
Build a cohesive C++23 platform that delivers:
1. Full-featured terminal LaTeX input and output,
2. Reliable numeric and symbolic computation pipeline,
3. Optional lightweight TUI editor/preview mode,
4. Extensible architecture for graphing, scripting, and plugins.

### A — Actions
1. **Establish core architecture**: unified AST, strict parse/eval/render boundaries.
2. **Ship rendering-first milestones**: fractions, roots, superscripts/subscripts, multiline layout.
3. **Add symbolic and numeric depth incrementally**: exact arithmetic, simplification, derivatives.
4. **Introduce lightweight TUI**: editor + live preview + autocomplete.
5. **Expand platform**: graphing, scripting, plugin APIs, optimization internals.

### R — Results (Target Outcomes)
- v0 usable CLI that handles practical LaTeX math expressions elegantly.
- Stable rendering engine with golden tests and terminal compatibility guarantees.
- Modular engine enabling symbolic/graphing extensions without rewrites.
- Distinctive "wow" UX that differentiates OverCalc from classic calculators.

---

## Product Pillars

1. **CLI First**
   - Single-command utility must always be first-class.
   - Every major feature should be invocable non-interactively.

2. **Math Engine Central**
   - Frontends call engine APIs; engine never depends on UI concerns.

3. **Renderer as Moat**
   - Layout quality is a competitive advantage and should be benchmarked/tested.

4. **Inspectable Internals**
   - Traces, step mode, IR dumps, and diagnostics are part of product value.

5. **Safe Extensibility**
   - Plugin and scripting systems should be constrained, versioned, and testable.

---

## System Architecture Plan

## 1) Frontends
- **CLI frontend** (primary): command parsing, file/stdin support, flags, output mode selection.
- **TUI frontend** (secondary): line editor, live render pane, suggestions, symbol picker.

## 2) Input Pipeline
- Input kind detection (LaTeX, expression syntax, script).
- Lexer/parser dispatch.
- Normalization pass to unified AST.

## 3) Core Math Representation
- Unified AST with source spans and annotations.
- Type tagging (numeric, symbolic, unit-aware, tensor-aware).
- Canonical forms for simplification and equality checks.

## 4) Compute Engines
- Numeric evaluator (exact + approximate modes).
- Symbolic engine (rewrite rules + transformation pipeline).
- Optimization passes (common subexpressions, simplification ordering).

## 5) Render Engine
- AST -> layout tree -> backend renderer.
- Unicode backend + ASCII fallback backend.
- Render targets: terminal panel, plain text, LaTeX output, JSON fragments.

## 6) Observability & Developer Tooling
- Trace flags (`--steps`, `--ast`, `--ir`, `--timing`).
- Structured diagnostics with source mapping and hints.
- Benchmark harness for parser/render/eval hotspots.

---

## Implementation Roadmap (Detailed)

## Phase 0 — Foundation (Weeks 1–4)
**Goal:** minimal vertical slice from parse to pretty output.

Deliverables:
- CMake project skeleton; test + benchmark targets.
- Basic expression parser (`+ - * / ^`, parentheses, variables).
- Initial LaTeX subset parser (`\\frac`, superscript, subscript, `\\sqrt`).
- Unified AST + basic evaluator.
- Boxed terminal output scaffold.
- Baseline CLI UX and error reporting.

Exit criteria:
- `overcalc '2+2'` works reliably.
- `overcalc '\\frac{1}{3}'` renders cleanly.
- CI runs tests and style checks.

## Phase 1 — Real Math Rendering (Weeks 5–10)
**Goal:** rendering quality becomes project signature.

Deliverables:
- 2D layout model: width, height, baseline, alignment.
- Nested fractions/roots with proper vertical alignment.
- Superscript/subscript stacking and operator precedence formatting.
- Greek symbol table + command map.
- Unicode capability detection and ASCII fallback.
- Golden/snapshot renderer tests.

Exit criteria:
- Complex nested expressions render correctly in common terminals.
- Renderer regressions caught automatically.

## Phase 2 — TUI Editor (Weeks 11–16)
**Goal:** lightweight math IDE feeling without heavy fullscreen complexity.

Deliverables:
- Single-pane editor + split live preview.
- Syntax highlighting for LaTeX tokens and math entities.
- Autocomplete engine (commands, functions, symbols).
- Snippet/template insertion (fraction, integral, sum, matrix).
- Symbol picker overlay (Ctrl+Space).

Exit criteria:
- TUI can compose non-trivial LaTeX expressions quickly.
- Latency remains low under frequent re-rendering.

## Phase 3 — Symbolic Core (Weeks 17–24)
**Goal:** practical symbolic workflows.

Deliverables:
- Exact rational arithmetic and canonical simplification.
- Rewrite-rule engine (simplify, expand, factor basics).
- Symbolic derivatives for core function families.
- `--steps` explanation mode for transformations.

Exit criteria:
- Symbolic outputs are stable, deterministic, and test-covered.

## Phase 4 — Graphing (Weeks 25–30)
**Goal:** terminal-native visualization.

Deliverables:
- 2D function plotting (ASCII + Unicode/braille modes).
- Domain/range controls and axes styling.
- Parametric + polar support (initial).
- CLI graph output export modes.

Exit criteria:
- Graphing integrated with same expression engine and parser.

## Phase 5 — Scientific Expansion (Weeks 31–38)
**Goal:** broaden practical utility.

Deliverables:
- Matrices/vectors/tensors primitives.
- Unit system and constants library.
- Statistical + linear algebra starter set.
- Numeric uncertainty and interval basics.

## Phase 6 — REPL + Scripting (Weeks 39–46)
**Goal:** programmable computational workflows.

Deliverables:
- Stateful REPL with history and sessions.
- User functions/macros.
- Lightweight script format with imports and pipeline operations.

## Phase 7 — Plugin Platform (Weeks 47–54)
**Goal:** domain extensibility.

Deliverables:
- Stable plugin API contracts.
- Sandbox/security model.
- Plugin registry metadata format.
- Example plugins (physics, finance, DSP).

## Phase 8 — Performance & Compiler Internals (Weeks 55–64)
**Goal:** scale and speed.

Deliverables:
- Expression IR.
- Optimization passes and caching.
- Optional JIT/SIMD experiments.
- Perf dashboards and regression gates.

## Phase 9 — AI Assistance (Weeks 65–72)
**Goal:** guided intelligence on top of deterministic engine.

Deliverables:
- Explain/suggest mode with safety boundaries.
- Derivation hints and simplification candidates.
- Prompt-to-math workflow hooks.

---

## Operating Routine (Execution Cadence)

## Daily Routine
1. **15 min triage**: bugs, flaky tests, render regressions.
2. **90–180 min deep work block**: one subsystem only.
3. **Renderer check gate**: run golden render suite before merge.
4. **Devlog entry**: decisions, assumptions, unresolved risks.

## Weekly Routine
1. **Monday planning**
   - Pick one phase objective + one stretch objective.
2. **Midweek architecture sync**
   - Validate interfaces and invariants.
3. **Friday demo**
   - Record CLI/TUI walkthrough from real examples.
4. **Retro**
   - Keep: working patterns.
   - Change: bottlenecks.
   - Kill: dead-end experiments.

## Monthly Routine
1. Milestone review against exit criteria.
2. Compatibility sweep (terminals, fonts, Unicode behavior).
3. Performance baseline update.
4. Roadmap re-scope by evidence (not optimism).

---

## Engineering Standards

- C++23 with strict warnings and sanitizers in debug builds.
- Deterministic output for tests and reproducibility.
- Source-span-aware diagnostics for all parser/eval errors.
- Golden tests for renderer and symbolic transformation snapshots.
- Benchmarks for parser throughput, render latency, and eval latency.

---

## Risk Register + Mitigations

1. **Renderer complexity explosion**
   - Mitigation: isolate layout primitives and snapshot tests early.
2. **CAS scope blow-up**
   - Mitigation: prioritize deterministic simplification kernels first.
3. **Terminal incompatibilities**
   - Mitigation: capability probing + ASCII fallback.
4. **Performance regressions**
   - Mitigation: CI benchmarks + budget thresholds.
5. **Feature creep fatigue**
   - Mitigation: enforce exit criteria before phase advancement.

---

## Definition of Done (Per Milestone)

A milestone is done only if:
- Feature works via CLI command examples.
- Tests include normal, edge, and failure cases.
- Errors are readable and actionable.
- Documentation updated with usage examples.
- Performance impact measured (if applicable).

---

## First 30-Day Action Plan

Week 1:
- Scaffold project layout, toolchain, lint/test harness.

Week 2:
- Implement core AST + expression parser + evaluator.

Week 3:
- Add LaTeX subset parser and mapping to unified AST.

Week 4:
- Implement first renderer pass + boxed output + snapshots.

**30-day success metric:**
A demo command set shows arithmetic + LaTeX fraction/root expressions with stable pretty terminal output and passing automated tests.
