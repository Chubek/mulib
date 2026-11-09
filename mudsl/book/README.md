# The Book of MuDSL

76 authored chapters: toolkit (1–37), implementation practice (38–45), MuLL
(46–70), and MuLLIER/library/host internals (71–76). Reference parts include
every Markdown file in the **repository-root** `manual/`, all `MANIFEST.yaml`
metadata, and full highlighted `MuDSL.hpp` and `MuDSL.d` source appendices.
The title page retains the original scaffold's attribution.

Build with `make -C book` (LuaLaTeX and latexmk), or run `texlua build.lua`
from this directory. The PDF is `build/the-book-of-mudsl.pdf`.
Requirements: Python 3.9+ with **PyYAML and Pygments**, LuaLaTeX, latexmk,
minted (including its Python executable for minted 3), and TeX Gyre/DejaVu fonts.
The modular typography suite in [`support/`](support/README.md) uses standard
TeX Live packages; its README lists dependencies, modules, and authoring helpers.
The Makefile enables shell escape for highlighting. `prepare_highlighting.py`
stages the MuLL Pygments plugin in `build/python`, exposed through `PYTHONPATH`;
no system Python installation is modified. The complete reference is large,
so allow several minutes for the first multi-pass PDF build.

Chapters 1–37 are exported from `mudsl/manual` using the supplied `md2latex.py`:
`make -C book export`; `make -C book check` verifies reproducibility.
Chapters 38–76 are authored directly in `chapters/`.
`mullcode` environments contain standalone, runnable MuLL examples and are
automatically checked by the `mull_conformance` CTest target.

`make generate` runs `typeset-manuals.py` and `typeset-manifest.py` before TeX.
Their outputs are under `build/generated`; each script accepts input/output
paths and `--check`. `make check` verifies generated content and runs book/lexer
regressions. `book_support.py` adapts the provided converter for minted listings,
wrappable inline identifiers, and multipage reference tables. The supplied
`md2latex.py` remains usable as a standalone converter.

## Typography

The book uses `support/mudsl-book.sty`, a coordinated suite of 30 style packages
covering page design, matching text/math fonts, headings, navigation, code,
multipage tables, and learning aids. `mudsl.sty` is now only a compatibility
loader. Existing chapter commands and code environments continue to work.

Run `make typography` (or `texlua build.lua typography`) for fast, multi-pass
specimens in `build/typography/`. This checks both the screen design and the
legacy loader's one-sided print profile, including table/listing continuation
and layout warnings. It additionally requires Poppler's `pdftotext`.
