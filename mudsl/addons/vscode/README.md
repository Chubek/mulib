# MuLL, MuDSL and MuLIB

MuLL syntax highlighting and language intelligence, with MuDSL C++/D and MuLIB C
snippets. Supports `*.mull` without changing C/C++/D file associations.

Python 3.9+ enables the bundled language server: completion, hover, definitions,
document symbols, and lexical diagnostics. Set `mu.checkerPath` to the separately
built `mull-check` for diagnostics from the real MuLL parser. Documents are parsed,
never executed. Set `mu.repositoryRoot` to a MuLIB or standalone MuDSL checkout
for local manuals and header navigation.

Commands: **Mu: Open Documentation**, **Mu: Restart Language Server**.
Settings: `mu.pythonPath`, `mu.repositoryRoot`, `mu.checkerPath`,
`mu.packagePaths`, `mu.nativeApi`.

Snippet prefixes include `val`, `fn`, `loadlib`, `mulib`, `mullhello`, `mudsl`,
`mudslparse`, `mullembed`, `mulibinclude`, `mulibimpl`, and `mulibvec`.
Embedded-host snippets include imports and a complete `main` function.

Build from the checkout with `sh addons/vscode/install.sh --package-only`.
Complete installation and other editor instructions are in `addons/README.md`.
