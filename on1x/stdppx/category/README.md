# Bundled native Category extensions

`Protocols` selects the explicit local scalar profile in
`../native/protocols.on1x` and `../../stdlib/ppx/category/{syntax,witness}.on1x`.
CMake compiles these without enabling Category itself. No Python processor
implements Category semantics.

Use `@enable Protocols` and `@extension Protocols { ... }`, or `--enable Protocols`.
Introspection: `--list-extensions`, `--explain-extension Protocols`. Registry:
`features.sexpr`; binary/schema hashes are pinned per build/install. Existing
`--stdppx` still selects the original Python PPXs.

Examples: `../examples/category/{eq,ordering}.on1x`. Full semantics, API, phase
split and limits: `../../docs/category.md`. Tests: `../../tests/test_category.py`.
Evidence: `../../VERIFICATION.md`.

Functors, generics, associated types, runtime dictionaries, derive, laws, auto
protocols and higher kinds remain unimplemented. Their registry entries are
unavailable; no fake native processors are supplied.
