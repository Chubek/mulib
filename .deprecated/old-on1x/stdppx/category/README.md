# Bundled native Category extensions

`Protocols` version 2 selects the checked local scalar profile in
`../native/protocols.on1x` and
`../../stdlib/ppx/category/{syntax,witness,metadata,typed}.on1x`.
CMake compiles these without enabling Category itself. No Python processor
implements Category semantics.

Use `@enable Protocols` and `@extension Protocols { ... }`, or `--enable Protocols`.
Introspection: `--list-extensions`, `--explain-extension Protocols`,
`--emit-category-plan INPUT`. Registry:
`features.sexpr`; binary/schema hashes are pinned per build/install. Existing
`--stdppx` still selects the original Python PPXs.

The native typed phase consumes host-inferred concrete member signatures and
returns a read-only plan. The compiler independently checks types, provenance,
completeness and selector forwarding. Reports carry source/artifact digests;
they are inspection output, not reusable proof inputs. Rebuild/install compiler,
processor and schema together: version-1 processor pins reject.

Examples: `../examples/category/{eq,ordering,evidence}.on1x`. Full semantics, API, phase
split and limits: `../../docs/category.md`. Tests: `../../tests/test_category.py`
and `../../tests/test_extension_evidence.cpp`.
Evidence: `../../VERIFICATION.md`.

Imported evidence/separate interfaces, functors, generics, associated types, runtime dictionaries, derive, laws, auto
protocols and higher kinds remain unimplemented. Their registry entries are
unavailable; no fake native processors are supplied.
