# On1x Book Toolkit

A self-contained, modular LaTeX distribution for authoring a technical book
about the On1x language, its toolchain, and the MuLIB ecosystem.

## Quick start

```sh
cd examples
export TEXINPUTS="../tex:${TEXINPUTS:-}"
latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex
```

For LuaLaTeX, replace `-pdf` with `-lualatex` (fonts remain portable).
The example uses `openany` for fewer blank pages; change to `openright` for print.

The glossary is opt-in: call `\OnxEnableGlossary` in the preamble and
`\OnxPrintGlossary` near the end, then run `makeglossaries` as required.

## Distribution

- `tex/on1x-book.cls`: two-sided book class; `screen` (default) or `print`,
  and `full` (default) or `light` package sets. The remaining options are passed
  to the standard `book` class (`11pt`, `12pt`, `oneside`, `twoside`, etc.).
- `tex/on1x-suite.sty`: aggregated module loader.
- `tex/on1x-*.sty`: independent modules, loadable individually in other classes.
- `examples/main.tex`: sample manuscript testing the main concepts.
- `docs/MODULES.json`: complete module descriptions and dependencies.
- `docs/API.md`: generated command and environment catalog.
- `docs/SOURCE-EVIDENCE.md`: provenance and caution around support claims.

## Profile behavior

`full` loads every component. `light` loads the foundational, code, formal and
cross-reference components suitable for a small manuscript. Independently
load an omitted module with `\usepackage{on1x-<name>}`.

Most modules use widely distributed TeX Live packages: `tcolorbox`, `listings`,
`tikz`, `forest`, `glossaries`, `imakeidx`, `amsmath`, `mathpartir`, `algorithm2e`,
`cleveref`, and others. Install a reasonably complete TeX Live distribution.

## Safety of On1x examples

The `listings` language profile highlights tokens but **does not validate**
compiler support. Style macros for typing judgments, inference rules,
concurrency, and proposed constructs express textbook notation, not executable
On1x guarantees. Distinguish verified/executable, partial, and absent support
by version, profile, source snapshot, and actual tests.

## Development

```
make check
make example
make clean
```

`check` statically checks all style modules and performs smoke-compilation of
the example and a broader feature smoke test; `latexmk` is required. No `-shell-escape` is required.
