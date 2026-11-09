# MuDSL book design system

The book's presentation lives in **31 small `.sty` files**, not in the document
class or in individual chapters. `mudsl-book.sty` is the entry point; the other
30 modules are its implementation. Build from `mudsl/book`, as with the existing
book workflow. No `TEXINPUTS` changes or system-wide installation are required.

```latex
\documentclass[12pt,a4paper]{the-book-of-mudsl}
\usepackage{support/mudsl-book} % or [print] for uncolored PDF links
\title{The Book of MuDSL}
\MuBookSubtitle{A C++ and D Toolkit, and the MuLL Language}
\author{D. B. Cooper}
\date{}
```

Existing documents using `\usepackage{mudsl}` load the same suite. The shim also
forwards `screen` and `print` options. Those options affect links, not the paper
size or syntax-highlighting palette. Class options still control point size,
paper size, `oneside`/`twoside`, and `openany`/`openright`.

## Design

* **Reading first:** 12 pt Pagella, matching Pagella mathematics, modest leading,
  optical margins, and an approximately 150 mm A4 text measure. Heros establishes
  a clear section hierarchy; scaled DejaVu Sans Mono keeps code distinct without
  overwhelming prose. There are no downloaded fonts or decorative assets.
* **A quiet page:** white paper, dark ink, restrained blue accents, light rules,
  binding-aware margins, outer folios, and bounded running titles. Long chapter
  titles wrap at openings and are truncated only in the running heads.
* **Navigable reference:** visible part dividers, consistent chapter openings,
  contents columns sized for three-digit chapters and four-digit page numbers,
  numbered PDF bookmarks, working links, and document metadata.
* **Readable technical material:** page-breakable code, visible continuation
  marks, quieter line numbers, repeated multipage table headers, open booktabs
  rules rather than grids, and aligned captions and footnotes. Whole-source
  appendices use footnote-sized code, not the smaller script size.
* **Semantic learning aids:** notes, cautions, definitions, objectives, examples,
  summaries, and exercises use named environments. Their labels, not just their
  colors, communicate their purpose. These are optional; existing prose is not
  rewritten or automatically boxed.

This is a visual design system, **not a claim of PDF/UA accessibility**. The
current minted/tcolorbox publishing pipeline does not produce a fully tagged PDF.

## Module map

Load the entry point rather than manually reconstructing its ordered imports.
Each module has a specific responsibility; change shared styling here instead
of adding local formatting to chapter sources.

| Module (`mudsl-*.sty`) | Responsibility |
| --- | --- |
| `book` | Suite options and dependency order |
| `engine` | Engine detection and shared infrastructure |
| `palette` | Semantic colors and the original `MuBlue`/`MuCode` aliases |
| `fonts` | Serif, sans, and literal-code font families |
| `math` | Matching math fonts, symbols, and display spacing |
| `page` | Stock-relative margins, binding space, and page furniture clearance |
| `microtype` | Protrusion, font microtypography, and small-cap tracking |
| `paragraphs` | Leading, paragraph rhythm, and body ink |
| `headings` | Section hierarchy, numbering depth, and heading spacing |
| `chapters` | Numbered, unnumbered, and appendix chapter openings |
| `parts` | Part-divider hierarchy |
| `titlepage` | Cover composition, subtitle, edition, and imprint |
| `runningheads` | Chapter/section marks, bounded headers, and folios |
| `contents` | Contents hierarchy and long-book number columns |
| `lists` | Enumerations, bullets, and reference descriptions |
| `inline` | Breakable literal identifiers, terms, and key names |
| `code` | minted defaults and all five existing code environments |
| `tables` | Multipage tables, column types, rules, and table notes |
| `captions` | Consistent figure and table captions |
| `floats` | Float placement and surrounding whitespace |
| `figures` | Bounded image inclusion and source notes |
| `footnotes` | Hanging notes, separator rules, and placement |
| `quotes` | Quotations and epigraphs |
| `boxes` | Breakable notes, cautions, and definitions |
| `pedagogy` | Objectives, summaries, numbered examples, and exercises |
| `backmatter` | Bibliography/index typography and verification notes |
| `pagination` | Widows, orphans, and empty inserted versos |
| `links` | Screen/print links, metadata, URLs, and bookmarks |
| `markdown` | Converter/Pandoc compatibility macros |
| `references` | Named, linked cross-references through cleveref |
| `manuals` | Compact reference chapters, source provenance, linked unnumbered headings, and identifier breaks |

## Author-facing commands

All existing APIs remain: `\api`, `\mdchar`, `\mdstrike`, `\tightlist`,
`\passthrough`, `\pandocbounded`, `code`, `mullcode`, `cppcode`, `dcode`, and
`rulecode`. Dynamic converter-created minted environments inherit the same
listing defaults. `\inputminted` continues to accept per-listing overrides.
`\api{...}` takes a **literal identifier**, not executable LaTeX, and permits
breaks between characters without inserting hyphens into the identifier.

```latex
\MuBookEdition{Second edition} % optional title-page text
\MuBookImprint{LANGUAGE ENGINEERING} % optional title-page eyebrow

\begin{MuObjectives}
  \item Compose a pipeline from independent features.
\end{MuObjectives}

\begin{MuDefinition}{pipeline}
  A sequence of transformations with explicit input and output contracts.
\end{MuDefinition}

\begin{MuNote}[Host-language detail]
  Read the ownership contract before capturing a runtime value.
\end{MuNote}

\begin{MuCaution} % optional custom title, as for MuNote
  A code fragment is not a substitute for the conformance suite.
\end{MuCaution}

\begin{MuExample}{Trace a value}\label{ex:trace}
  Follow each transformation in order.
\end{MuExample}

\begin{MuExercises}
  \item\label{exercise:trace} Explain the result of \cref{ex:trace}.
\end{MuExercises}

\begin{MuSummary}
  \item Composition preserves the contract between stages.
\end{MuSummary}
```

Other helpers: `\MuTerm{term}`, `\MuKey{key}`, `\MuEpigraph{text}{attribution}`,
`\MuTableNote{text}`, `\MuFigure[graphicx options]{file}`,
`\MuFigureSource{text}`, and `\MuSourceNote{text}`. `\MuFigure` bounds images to
the available width and 68% of the text height by default. Fixed-width table
column types `L{width}`, `C{width}`, and `R{width}` provide ragged/centered text.
Use `\cref`, `\Cref`, and `\cpageref` for semantic references; `\MuChapterRef`
and `\MuPageRef` are convenience aliases.

The Markdown adapter in `../book_support.py` supplies the repeated headers and
continuation footers required by generated `xltabular` tables. It changes only
rendered presentation, not source Markdown or code blocks. Run `make export`
after changing that adapter; never hand-edit the exported toolkit chapters.

## Dependencies and verification

Primary engine: **LuaLaTeX**, with LaTeX 2022-06-01 or newer. The suite is tested
with TeX Live 2024. It uses standard TeX Live packages: geometry, microtype,
titlesec/titletoc, fancyhdr, truncate, enumitem, mathtools, unicode-math,
ragged2e, minted/fvextra/upquote, array/booktabs/longtable/tabularx/xltabular,
caption, footmisc, csquotes, tcolorbox (skins/breakable), hyperref, bookmark,
xurl, cleveref, etoolbox, calc, and needspace. The font modules retain a pdfTeX
fallback (newpxtext/newpxmath, helvet, beramono), but the complete Unicode manual
is intended for LuaLaTeX. XeLaTeX is not the publishing/test engine.

Python/Pygments and minted's shell-escape requirements are unchanged. Poppler's
`pdftotext` is additionally required for the typography smoke checks.

From `mudsl/book`:

```sh
make check       # generator, lexer, package-structure, and table regressions
make typography  # two small, multi-pass visual specimens and layout checks
make             # complete book, reference manuals, and source appendices
```

The specimen source is `typography-smoke.ltx`. `check_typography.py` compiles it
both through the direct suite (12 pt, A4, two-sided, recto openings, screen links) and through
the legacy loader (11 pt, letter, one-sided, print links). It rejects overfull
boxes, missing glyphs, font warnings, bookmark-string warnings, header-size
warnings, duplicate destinations, and unresolved references. PDF text checks
verify that long tables retain their last row and repeat their header, that
multi-page source listings reach their last line, and that code retains literal
quotes and hash characters. The specimen also stresses long titles, three-digit
chapter numbers, and four-digit folios. Outputs are
`build/typography/screen.pdf` and `build/typography/legacy-print.pdf`.

Inspect those PDFs as well as representative full-book pages after changing
fonts, dimensions, or counters. Automated checks are not a substitute for visual
review, and no chapter's technical-review status is changed by this suite.

The real manual generator is also exercised by the specimens: document titles,
local/fragment links, duplicate headings, expanded HTML disclosures, preserved
literal code, weighted multipage tables, and style restoration before appendices.
`manual_support.py` provides namespaced targets for all imported manuals, so
links such as `#a` and `#direct-dependencies` no longer become unresolved PDF
destinations. An unknown fragment falls back to its manual opening with an
explicit generation warning; external web links are not rewritten.
