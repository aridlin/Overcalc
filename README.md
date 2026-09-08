# OverCalc

OverCalc is a terminal-native math engine with LaTeX-friendly input, exact arithmetic,
symbolic simplification, differentiation, Unicode rendering, JSON output, batch mode,
and an early interactive editor.

```powershell
.\overcalc.exe "((2 + 3)^2 - 9) / 4"
```

```text
OverCalc - Unicode math - color output
+- OverCalc / Input ------------------------------------------------+
| ((2 + 3)^2 - 9) / 4                                               |
+-------------------------------------------------------------------+
+- Pretty Render ---------------------------------------------------+
|                         (2 + 3)^2 - 9                             |
|                         -------------                             |
|                               4                                   |
+-------------------------------------------------------------------+
+- Result ----------------------------------------------------------+
| Exact    4                                                        |
+-------------------------------------------------------------------+
```

## Build

```powershell
cmake -S . -B build-mingw
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```

Requires CMake 3.20+ and a C++23 compiler.

## Quick Start

Run a single expression:

```powershell
.\build-mingw\overcalc.exe "sin(pi/2)+50%"
```

Use LaTeX input:

```powershell
.\build-mingw\overcalc.exe "\frac{1}{3} + \frac{1}{6}"
.\build-mingw\overcalc.exe "\sqrt[3]{8}"
.\build-mingw\overcalc.exe "\left(2+3\right)^2"
```

Show steps:

```powershell
.\build-mingw\overcalc.exe --steps "sqrt(16)+x*0"
```

Differentiate:

```powershell
.\build-mingw\overcalc.exe --derive x "x^2 + sin(x)"
.\build-mingw\overcalc.exe --json --steps --derive x "x^3"
```

Batch evaluate a file:

```text
# formulas.txt
2+2
sin(pi/2)+50%
x^2
```

```powershell
.\build-mingw\overcalc.exe --batch --file formulas.txt
.\build-mingw\overcalc.exe --batch --derive x --file formulas.txt
```

## Interactive TUI Mode

Start the visual equation editor:

```sh
overcalc --tui
```

Type directly into the displayed equation. Fractions have editable numerator and
denominator fields; powers and subscripts appear above/below the base. Empty
fields show `□`. Structures can be nested without writing LaTeX.

The bottom insertion palette displays clickable miniature equations in bordered
cards, with page arrows for additional templates. It includes fractions, superscripts, subscripts, square
and indexed roots, brackets, absolute values, trig/log functions, constants,
multiplication, percent, and factorial. Click a button, or press **Ctrl+P**, use
arrows to choose, and press Enter. On small terminals, keyboard navigation reveals
additional pages. You can also click the page arrows.

- **Tab / Shift+Tab**: move between fields and out of a structure.
- **Left / Right**: move through values and enter nested structures.
- **Up / Down**: move between numerator/denominator or base/exponent.
- **Mouse click**: place the caret directly in the equation.
- **Home / End**: start/end of the current field.
- **Backspace / Delete**: remove a value or adjacent structure; at field edges,
  move across the boundary. **Ctrl+Z / Ctrl+Y** undo/redo edits.
- **Ctrl+F**: fraction; **Ctrl+E**: superscript; **Ctrl+R**: square root;
  **Ctrl+B**: subscript; **Ctrl+G**: brackets.
- Typing **/** or **^** wraps the preceding value as a fraction base/numerator
  or power base. Typing **(** inserts brackets; **)** leaves the field.
- **Ctrl+U**: clear; **Ctrl+D**: toggle derivative with respect to x.
- **Esc / Ctrl+C**: exit and restore the terminal. Esc closes an open palette first.

For example, type `12/4` to build a stacked fraction with result 3. Press Tab to
leave its denominator, then type `+2^3` for a result of 11. To build a fraction
from empty fields, choose Fraction, type the numerator, press Tab, and type the
denominator. Results update while editing; incomplete fields remain editable.

The canvas scrolls to keep the caret visible and the palette follows terminal
resizes. A terminal of at least 40 columns by 18 rows is required. The editor uses
Unicode fraction bars, radicals, tall parentheses, Greek letters, and spaced math
operators. Its centered equation panel highlights the active field; a separate
result panel displays exact and approximate values. The default dark background
keeps the interface readable in transparent terminals; `--no-color` disables the
color theme.

Piped `--tui` input retains the line-oriented preview and commands (`:derive x`,
`:derive off`, `:clear`, `:q`). Regular CLI input still accepts LaTeX.

After building, install the current binary so an older copy on PATH cannot hide
new flags:

```sh
cmake --install build --prefix "$HOME/.local"
# Ensure ~/.local/bin is on PATH before /usr/bin.
```

## CLI Options

```text
overcalc [options] '<expr>'
```

| Option | Description |
| --- | --- |
| `--ascii` | Use ASCII rendering |
| `--unicode` | Use Unicode rendering, the default |
| `--no-color` | Disable ANSI color |
| `--width N` | Set panel width |
| `--latex-output` | Print normalized LaTeX and result |
| `--json` | Print structured JSON |
| `--ast-json` | Print AST JSON |
| `--timing` | Print elapsed execution time |
| `--batch` | Evaluate one expression per input line |
| `--tui` | Start interactive editor/preview mode |
| `--derive VAR` | Differentiate with respect to `VAR` |
| `--diff VAR` | Alias for `--derive` |
| `--derivative VAR` | Alias for `--derive` |
| `--steps` | Show evaluation steps |
| `--file PATH` | Read expression input from a file |
| `--stdin`, `-` | Read expression input from stdin |
| `--list-symbols` | List supported LaTeX symbols |
| `--version` | Print version |
| `--help`, `-h` | Show help |

## Supported Input

Arithmetic:

```text
2 + 3 * 4
((2 + 3)^2 - 9) / 4
2pi
20% * 80
5!
```

LaTeX-style syntax:

```text
\frac{1}{2}
\sqrt{16}
\sqrt[3]{8}
\alpha_1 + \beta_2
\left|-3\right|
2\,\cdot\quad3
```

Functions and constants:

```text
sin(pi/2)
cos(\pi)
tan(x)
ln(e)
log(x)
exp(x)
abs(-3)
```

Differentiation currently supports constants, symbols, sums, differences, products,
quotients, numeric powers, `sin`, `cos`, `tan`, `ln`/`log`, `exp`, `sqrt`, and percent.

## JSON Output

```powershell
.\build-mingw\overcalc.exe --json --steps --derive x "x^2"
```

```json
{
  "line": 1,
  "input": "x^2",
  "ok": true,
  "latex": "x^{2}",
  "simplified": "2 * x",
  "simplified_latex": "2\\cdotx",
  "derivative": "2 * x",
  "derivative_latex": "2\\cdotx",
  "exact": "2 * x",
  "decimal": null,
  "steps": ["Start evaluation", "Derivative d/dx: 2 * x", "Symbolic exact form: 2 * x"]
}
```

## Roadmap

The active direction is:

- stronger simplification and symbolic forms
- richer derivative rules
- a more capable interactive TUI with selection menus and history
- golden renderer tests
- graphing and REPL/session support

See `OVERCALC_MASTER_PLAN.md` for the long-form roadmap.
