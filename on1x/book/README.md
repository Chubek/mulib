# On1x — Language, Compiler, and MuLIB Kernels

The manuscript has 120 chapters in 12 parts, from a first native program
through the language, standard library, project driver, compiler, backends,
foreign contracts, MuLIB kernels, and the compiler and runtime SDKs.
`on1x-book.ltx` orders the individual files in `chapters/`. Each chapter
links to the source, tests, or manuals that support its implementation claims.

For a hands-on path through ownership, start with the
[list example](examples/consume-list.on1x) in chapter 26, then read chapters
29–30 on `defer` and `Result`. [Appendix A](chapters/appendix-ownership.ltx)
uses a live-box counter to trace a returned owner, an early error, and an
owned error. It includes a one-line experiment and a compiler rejection
you can reproduce with the direct C11 compiler from chapter 3.

## Build the book

From the MuLIB repository root, run:

```sh
make -C on1x/book book
```

The output is `on1x/book/build/on1x-book.pdf`. The manuscript uses pdfLaTeX,
`latexmk`, and `makeindex`, with the standard TeX Live LaTeX and LaTeX Extra
packages and Latin Modern fonts. Build inside a checkout because some listings
include the On1x examples directly. The PDF includes a linked table of
contents, chapter cross-references, and a subject index.

## Use the authoring toolkit

```sh
cd examples
export TEXINPUTS="..:../tex:${TEXINPUTS:-}"
latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex
```

For LuaLaTeX, replace `-pdf` with `-lualatex` (fonts remain portable).
The example uses `openany` for fewer blank pages; change to `openright` for print.

The glossary is opt-in: call `\OnxEnableGlossary` in the preamble and
`\OnxPrintGlossary` near the end, then run `makeglossaries` as required.

## Distribution

- `on1x-book.cls`: two-sided book class with `screen` (default) or `print`,
  and `full` (default) or `light` package sets. The remaining options are passed
  to the standard `book` class (`11pt`, `12pt`, `oneside`, `twoside`, etc.).
  The `manuscript` option loads the styles used by the main book.
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

`check` builds the manuscript, checks the style modules, and compiles the
toolkit example and feature smoke test. The toolkit smoke tests additionally
require the full style dependencies listed above. No `-shell-escape` is required.
