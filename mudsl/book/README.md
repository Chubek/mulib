# The Book of MuDSL

79 authored chapters: toolkit (1–37), implementation practice (38–45), MuLL
(46–70), MuLLIER/library/host internals (71–76), and the CLI/Mu3AC native pipeline
(77–79). Reference parts include every Markdown file in the **repository-root**
`manual/` and full highlighted `MuDSL.hpp` and `MuDSL.d` source appendices.
The repository manifest is not rendered or required by the book build.
The title page retains the original scaffold's attribution.

Build with `make -C book` (LuaLaTeX and latexmk), or run `texlua build.lua`
from this directory. The PDF is `build/the-book-of-mudsl.pdf`.
Requirements: Python 3.9+ with **Pygments**, LuaLaTeX, latexmk,
minted (including its Python executable for minted 3), and TeX Gyre/DejaVu fonts.
The modular typography suite in [`support/`](support/README.md) uses standard
TeX Live packages; its README lists dependencies, modules, and authoring helpers.
The Makefile enables shell escape for highlighting. `prepare_highlighting.py`
stages the MuLL Pygments plugin in `build/python`, exposed through `PYTHONPATH`;
no system Python installation is modified. The complete reference is large,
so allow several minutes for the first multi-pass PDF build.

Chapters 1–37 are exported from `mudsl/manual` using the supplied `md2latex.py`:
`make -C book export`; `make -C book check` verifies reproducibility.
Chapters 38–79 are authored directly in `chapters/`.
`mullcode` environments contain standalone, runnable MuLL examples and are
automatically checked by the `mull_conformance` CTest target.

`make generate` runs `typeset-manuals.py` for the root manuals and the standard
library catalogue before TeX. Outputs are under `build/generated`; the script
accepts input/output paths and `--check`. `make check` verifies generated content
and runs book/lexer/manual-rendering regressions. The supplied `md2latex.py`
remains usable as a dependency-free standalone converter.

## Printed manuals

`typeset-manuals.py`, `manual_support.py`, and `book_support.py` give the imported
manuals a dedicated reference layout without changing the Markdown:

- Reading guides first, then the alphabetical module reference; all files remain
  included, in deterministic order independent of the manifest.
- A document's actual title replaces the filename heading and duplicate H1.
  A compact chapter opener records the source filename separately.
- Unnumbered sections retain clickable PDF bookmarks without overwhelming the
  global contents with hundreds of duplicate subsection entries.
- Markdown-to-Markdown and fragment links become real PDF links, with unique,
  checkout-independent targets, including repeated headings and separate catalogues.
- HTML comments/maintenance markers are hidden; `details`/`summary` blocks expand
  for print. Their contents, code examples, and verbatim header excerpts remain.
- Inline code prefers punctuation/case boundaries, with penalized fallback for
  long identifiers. Multipage tables repeat headers and gently weight columns
  according to their contents.

Repository-file links are rebased for the PDF's `build/` location, and local
images for TeX's working directory; external web destinations are preserved.
Internal manual links remain portable, while links to repository files require
that source tree. Unknown fragments link
to the corresponding manual opening and produce an explicit generation warning.
Changes to standalone converter rendering hooks preserve its default output;
`export_manual.py --check` guards the existing toolkit chapter exports.

## Typography

The book uses `support/mudsl-book.sty`, a coordinated suite of 31 style packages
covering page design, matching text/math fonts, headings, navigation, code,
multipage tables, and learning aids. `mudsl.sty` is now only a compatibility
loader. Existing chapter commands and code environments continue to work.

Run `make typography` (or `texlua build.lua typography`) for fast, multi-pass
specimens in `build/typography/`. This checks both the screen design and the
legacy loader's one-sided print profile, including table/listing continuation
and layout warnings. It additionally requires Poppler's `pdftotext`.
