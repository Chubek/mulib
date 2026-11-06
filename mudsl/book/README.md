# The Book of MuDSL

70 chapters: toolkit (1–37), implementation practice (38–45), MuLL (46–70).
The title page retains the original scaffold's attribution.

Build with `make -C book` (LuaLaTeX and latexmk), or run `texlua build.lua`
from this directory. The PDF is `build/the-book-of-mudsl.pdf`.
The build uses standard TeX Live packages and installed TeX Gyre/DejaVu fonts;
it needs neither shell escape nor bibliography/index programs.

Chapters 1–37 are exported from the existing Markdown manual with Pandoc:
`make -C book export`; `make -C book check` verifies reproducibility.
Chapters 38–70 are authored directly in `chapters/`.
`mullcode` environments contain standalone, runnable MuLL examples and are
automatically checked by the `mull_conformance` CTest target.
