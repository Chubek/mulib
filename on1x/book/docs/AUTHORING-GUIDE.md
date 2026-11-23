# Authoring Guide: A Book About On1x

## Recommended organization

1. **Foundations**: the motivation for On1x, MuLIB, toolchain overview, installation,
   lexing/parsing, and the distinction between spec, implemented profile, and verified behavior.
2. **Core programming**: values, literals, types, functions, branching, patterns,
   modules, errors, ownership, interoperability, and standard-library interfaces.
3. **Formal language reference**: lexical structure, grammar, evaluation judgments,
   typing rules, effect notation, and carefully scoped correctness claims.
4. **Runtime/toolchain**: source to HIR, verifications, optimizations, MIR and C11 paths,
   runtime profiles, module loading, PPX, and FFI/kernel admission.
5. **Advanced topics**: cooperative concurrency, channels, program structure,
   compiler implementation, testing and diagnostics.
6. **Appendices**: catalog of built-in and optional constructs, glossary, API references,
   commands, changelog, and evidence ledger.

## The status convention

A language keyword in `listings` does **not** establish parser or backend support.
Include a source revision and profile in each status table.
Use `\OnxExecutable`, `\OnxPartial` or `\OnxAbsent` only with evidence.
A source file, contract, manifest, kernel implementation, ABI or hash is not
by itself proof of an admitted On1x binding. Prefer failing closed on unknown
admission, and spell out testing limitations.

## Book mechanics

```tex
\documentclass[11pt,twoside,openright,print,full]{on1x-book}
\OnxBookTitle{The On1x Programming Language}
\OnxBookAuthor{Your Name}
\begin{document}
\frontmatter
\maketitle
\OnxContents
\mainmatter
\chapter{Functions}
\begin{OnxCodeBlock}[caption={Illustrative On1x syntax}]
fn identity(x: i64) -> i64 { return x; }
\end{OnxCodeBlock}
\backmatter
\OnxPrintIndex
\end{document}
```

For inline program fragments use `\OnxInline|...|`, not `\verb` in a
caption or moving argument. For files, `\OnxCodeFile[linerange={3-12}]{foo.on1x}`
uses standard `listings` options; prefer explicit `firstline` and `lastline`
for reproducible excerpts. Source paths in the PDF are displayed using
`\OnxFile{path}`.

Use `\OnxGrammar`/`\OnxProduction` for syntax; `\OnxTyping`, `\OnxRules`,
`\OnxRule` for formal judgments; `\OnxDiagram` and `\OnxAST` for figures.
Treat theorem rules as textbook formalizations unless the source and tests
justify calling them normative.

For a glossary call `\OnxEnableGlossary` in the preamble, then define your
entries and run `makeglossaries` before printing the result. For indexing,
`\OnxIndex{...}` records terms and `\OnxPrintIndex` prints them.

## Edition discipline

Maintain a revision, date, and test profile for every snapshot of On1x.
Keep design proposals and partial implementations separate from documented
runnable features; regenerate capability tables for each edition.
