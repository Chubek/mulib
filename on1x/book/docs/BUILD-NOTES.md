# Build and validation notes

This kit was checked with the TeX Live tools available in the generation environment.

- `make check`: module-manifest validation, pdfLaTeX sample manuscript, and a
  pdfLaTeX feature smoke test exercising grammar, syntax trees, pseudocode,
  diagrams, tables, checklists, kernel cards, protocol panels and more.
- `tests/light-smoke.tex`: pdfLaTeX **and** LuaLaTeX tested for the `light` profile.
- Main example: 12-page PDF from `examples/main.tex`.
- The toolkit smoke tests typeset illustrative On1x. Execution evidence for
  the manuscript's ownership lab is recorded in [SOURCE-EVIDENCE.md](SOURCE-EVIDENCE.md).
- Full coverage of every public macro in every possible package combination
  is not claimed; the sources are editable and the tests are extensible.

## Dependencies

A TeX Live distribution with `texlive-latex-extra`, `texlive-pictures`,
`texlive-science` and their dependencies is generally suitable. In particular,
`tcolorbox`, `forest`, `tikz`, `mathpartir`, `cleveref`, `algorithm2e`, `glossaries`,
`imakeidx`, `fancyhdr`, `titlesec` and `listings` are used. No shell-escape
is required for the code-listings backend.

## Reproducing the checks

```sh
make check
TEXINPUTS="$PWD/tex:" lualatex -halt-on-error \
    -output-directory=tests tests/light-smoke.tex
```

The resulting PDF files are build artifacts, not authoritative specifications
of language behavior.
