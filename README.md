# OverCalc

A terminal-native computational engine that makes beautiful math rendering the default. LaTeX-first input, exact rational arithmetic, symbolic differentiation, and Unicode output — all from the command line.

```
$ overcalc "((2 + 3)^2 - 9) / 4"

OverCalc • Unicode math • color output
╭─ OverCalc / Input ──────────────────────────────────────────╮
│ ((2 + 3)^2 - 9) / 4                                         │
╰─────────────────────────────────────────────────────────────╯
╭─ Pretty Render ─────────────────────────────────────────────╮
│                                                              │
│                    (2 + 3)² - 9                              │
│                    ────────────                              │
│                         4                                    │
│                                                              │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Exact    4                                                   │
╰─────────────────────────────────────────────────────────────╯
```

---

## Features

- **LaTeX input** — `\frac`, `\sqrt`, `^`, `_`, `\alpha`, `\pi`, and 70+ symbols
- **Exact rational arithmetic** — results like `1/3`, `7/12`, never lossy floats by default
- **Symbolic simplification** — `x + 0` → `x`, `1 * x` → `x`
- **Symbolic differentiation** — chain rule, product rule, trig derivatives
- **Multi-panel Unicode output** — input, pretty render, simplified form, result
- **ASCII fallback** — `--ascii` for limited terminals
- **Syntax highlighting** — numbers, operators, functions, Greek symbols in color
- **Structured output** — `--json`, `--latex-output`, `--ast-json` for tooling
- **Batch processing** — evaluate a file of expressions line by line

---

## Build

```sh
mkdir build && cd build
cmake ..
cmake --build .
```

Requires C++23 and CMake 3.20+. The build produces the `overcalc` executable and the `overcalc_core` static library.

---

## Usage

```
overcalc [options] '<expr>'
```

### Options

| Flag | Description |
|------|-------------|
| `--ascii` | ASCII-only rendering (no Unicode) |
| `--unicode` | Unicode rendering (default) |
| `--no-color` | Disable ANSI colors |
| `--width N` | Set panel width (clamped to 68–120) |
| `--derive VAR` | Differentiate with respect to VAR |
| `--steps` | Show evaluation steps |
| `--json` | Structured JSON output |
| `--latex-output` | Normalized LaTeX and result |
| `--ast-json` | Print AST as JSON |
| `--batch` | One expression per input line |
| `--file PATH` | Read expression from a file |
| `--stdin`, `-` | Read expression from stdin |
| `--list-symbols` | List all supported LaTeX symbols |
| `--timing` | Print elapsed time (ms) to stderr |
| `--version` | Print version |
| `--help`, `-h` | Show help |

---

## Expression Syntax

### Arithmetic

```
$ overcalc "2 + 3 * 4 - 1"

╭─ Result ────────────────────────────────────────────────────╮
│ Exact    13                                                  │
╰─────────────────────────────────────────────────────────────╯
```

Standard operators: `+` `-` `*` `/` `^` (power). Parentheses group sub-expressions. Unary `-` is supported.

### Fractions

Use `\frac{num}{den}` for vertical fraction rendering:

```
$ overcalc "\frac{1}{3} + \frac{1}{6}"

╭─ Pretty Render ─────────────────────────────────────────────╮
│                                                              │
│                   1       1                                  │
│                   ─   +   ─                                  │
│                   3       6                                  │
│                                                              │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Exact    1/2                                                 │
╰─────────────────────────────────────────────────────────────╯
```

Slash division `/` also renders as a vertical fraction in the pretty panel.

### Square Roots and nth Roots

```
$ overcalc "\sqrt{1 + 2 + 5 + 8}"

╭─ Pretty Render ─────────────────────────────────────────────╮
│                  ─────────────                               │
│                √ 1 + 2 + 5 + 8                               │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Exact    4                                                   │
╰─────────────────────────────────────────────────────────────╯

$ overcalc "\sqrt[3]{8}"

╭─ Pretty Render ─────────────────────────────────────────────╮
│                       ───                                    │
│                     3√ 8                                     │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Exact    2                                                   │
╰─────────────────────────────────────────────────────────────╯
```

### Trigonometry and Constants

```
$ overcalc "sin(pi/2) + 50%"

╭─ Pretty Render ─────────────────────────────────────────────╮
│                                                              │
│                       π                                      │
│                sin(   ─   ) + 50%                            │
│                       2                                      │
│                                                              │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Exact    3/2                                                 │
╰─────────────────────────────────────────────────────────────╯
```

Available functions: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `ln`, `log`, `abs`.  
Constants: `pi` / `\pi` ≈ 3.14159…, `e` ≈ 2.71828…  
Postfix: `!` (factorial), `%` (percent — divides by 100).

### Greek Symbols and Subscripts

```
$ overcalc "\alpha_1 + \beta_2"

╭─ Pretty Render ─────────────────────────────────────────────╮
│                      α  + β                                  │
│                       1    2                                 │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Form     α₁ + β₂                                            │
╰─────────────────────────────────────────────────────────────╯
```

Superscripts `^` and subscripts `_` accept braces for multi-character spans: `x^{10}`, `a_{ij}`.  
Implicit multiplication: `2pi` → `2·π`, `3x` → `3·x`.

---

## Symbolic Differentiation

Pass `--derive VAR` (aliases: `--diff`, `--derivative`) to compute a symbolic derivative:

```
$ overcalc --derive x "x^3 + 2*x^2 - x + 7"

╭─ OverCalc / Input ──────────────────────────────────────────╮
│ x^3 + 2*x^2 - x + 7                                         │
╰─────────────────────────────────────────────────────────────╯
╭─ Pretty Render ─────────────────────────────────────────────╮
│                   3      2                                   │
│                  x  + 2·x  - x + 7                           │
╰─────────────────────────────────────────────────────────────╯
╭─ Derivative d/dx ───────────────────────────────────────────╮
│                         2                                    │
│                   3·x  + 4·x - 1                             │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Form     3 * x^2 + 4 * x - 1                                │
╰─────────────────────────────────────────────────────────────╯
```

```
$ overcalc --derive x "sin(x) * x^2"

╭─ Derivative d/dx ───────────────────────────────────────────╮
│                  sin(x) · 2·x + cos(x) · x²                 │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Form     sin(x) * 2 * x + cos(x) * x^2                      │
╰─────────────────────────────────────────────────────────────╯
```

---

## Simplification Steps

Add `--steps` to trace how the expression was evaluated:

```
$ overcalc --steps "x + 0 + 1*x"

╭─ OverCalc / Input ──────────────────────────────────────────╮
│ x + 0 + 1*x                                                  │
╰─────────────────────────────────────────────────────────────╯
╭─ Simplified ────────────────────────────────────────────────╮
│                         2·x                                  │
╰─────────────────────────────────────────────────────────────╯
╭─ Result ────────────────────────────────────────────────────╮
│ Form     2 * x                                               │
╰─────────────────────────────────────────────────────────────╯

╭─ Steps ─────────────────────────────────────────────────────╮
│ › Start evaluation                                           │
│ › Simplified: 2 * x                                         │
╰─────────────────────────────────────────────────────────────╯
```

---

## Output Formats

### JSON

```
$ overcalc --json "sin(pi/6)"
{
  "line": 1,
  "input": "sin(pi/6)",
  "ok": true,
  "latex": "\\sin(\\frac{\\pi}{6})",
  "simplified": "sin(pi / 6)",
  "simplified_latex": "\\sin(\\frac{\\pi}{6})",
  "exact": "1/2",
  "decimal": null,
  "steps": []
}
```

Add `--steps` to populate the `steps` array. Add `--timing` to include `timing_ms`.

### LaTeX Output

```
$ overcalc --latex-output "\frac{1}{3} + \frac{1}{6}"
\frac{1}{3}+\frac{1}{6} = 1/2

$ overcalc --latex-output --derive x "x^2"
x^{2} \xrightarrow{d/dx} 2 \cdot x = 2 * x
```

### AST JSON

```
$ overcalc --ast-json "x^2 + 1"
{"Binary": {"+", {"Superscript": {"base": {"Symbol": "x"}, "exponent": {"Number": 2}}}, {"Number": 1}}}
```

---

## Batch Processing

Process multiple expressions from a file or stdin — one expression per line. Lines starting with `#` are treated as comments.

**formulas.txt**
```
# geometry
pi * 3^2
\frac{4}{3} * pi
2 + 2
\sqrt{144}
```

```
$ overcalc --batch --file formulas.txt
1  pi * 3^2        = 9 * pi  ~= 28.2743338823
2  \frac{4}{3}*pi  = 4/3 * pi  ~= 4.18879020479
3  2 + 2            = 4
4  \sqrt{144}       = 12
```

Pipe from stdin:

```
$ echo -e "2+2\nsin(pi/2)+50%\n1/3+1/6" | overcalc --stdin --batch
1  2+2             = 4
2  sin(pi/2)+50%   = 3/2
3  1/3+1/6         = 1/2
```

Combine with `--json` for structured batch output:

```
$ overcalc --batch --json --file formulas.txt
[
  { "line": 1, "input": "pi * 3^2", "ok": true, ... },
  { "line": 2, "input": "\\frac{4}{3} * pi", "ok": true, ... },
  ...
]
```

---

## ASCII Mode

Use `--ascii` on terminals without Unicode support:

```
$ overcalc --ascii "\frac{1}{2} + \frac{1}{3}"

OverCalc - ASCII math - color output
+-- OverCalc / Input -------------------------------------------+
| \frac{1}{2} + \frac{1}{3}                                     |
+---------------------------------------------------------------+
+-- Pretty Render ----------------------------------------------+
|                   1       1                                   |
|                   -   +   -                                   |
|                   2       3                                   |
+---------------------------------------------------------------+
+-- Result -----------------------------------------------------+
| Exact    5/6                                                  |
+---------------------------------------------------------------+
```

---

## Supported Symbols

```
$ overcalc --list-symbols

Supported LaTeX Symbols
  \alpha        alpha         α
  \beta         beta          β
  \gamma        gamma         γ
  \delta        delta         δ
  \epsilon      epsilon       ε
  \zeta         zeta          ζ
  \eta          eta           η
  \theta        theta         θ
  \iota         iota          ι
  \kappa        kappa         κ
  \lambda       lambda        λ
  \mu           mu            μ
  \nu           nu            ν
  \xi           xi            ξ
  \pi           pi            π
  \rho          rho           ρ
  \sigma        sigma         σ
  \tau          tau           τ
  \phi          phi           φ
  \chi          chi           χ
  \psi          psi           ψ
  \omega        omega         ω
  \cdot         cdot          ·
  \times        times         ×
  \div          div           ÷
  ... (70+ total, see --list-symbols)
```

Typos produce a suggestion: `\alpa` → `did you mean \alpha?`

---

## Error Reporting

Parse errors report the column and point to the problem:

```
$ overcalc "2 + * 3"
parse error at column 5: unexpected token '*'
2 + * 3
    ^

$ overcalc "\sqt{4}"
parse error at column 1: unknown command '\sqt' — did you mean '\sqrt'?
\sqt{4}
^
```

---

## Timing

```
$ overcalc --timing "\sqrt[3]{1000000}"
╭─ Result ────────────────────────────────────────────────────╮
│ Exact    100                                                 │
╰─────────────────────────────────────────────────────────────╯
timing_ms 0.041
```

Timing is written to stderr so it does not interfere with piped output.

---

## Environment

| Variable | Effect |
|----------|--------|
| `COLUMNS` | Terminal width hint (Linux/macOS) |

On Windows, UTF-8 console output and ANSI virtual terminal processing are enabled automatically.

---

## License

MIT
