# Standard PPX core

Six processors have both native On1x (`native/NAME.on1x`) and Python bootstrap
(`NAME.py`) implementations, invoked by `on1xc --ppx LABEL FILE`:

| Program | Input / behavior |
|---|---|
| `identity.py` | Validate and round-trip the AST |
| `checks.py` | `@label("assert_eq", a, b)` / `assert_ne`; hygienic intrinsic wrappers |
| `control.py` | Statement `@label("when", condition, {body})` / `unless` |
| `unroll.py` | Statement `@label("repeat", literal_count, {body})`, count 0..256 |
| `derive.py` | Function attribute `@label("derive", "arity", "signature")` |
| `fold.py` | Fold representable literal arithmetic without hiding runtime traps |

The assigned label comes from `ON1X_PPX_LABEL`, not a hard-coded name.
`examples/ppx.on1x` composes five of these using source registrations; it needs
`--allow-source-ppx`. No chmod is required for these shebang scripts.

The default CMake build emits `BUILD/stdppx/on1x-ppx-NAME`. Native executables
require `ON1X_PPX_SCHEMA` to name the absolute `grammar/PPX.schema.json` path;
no Python/compiler/LibStone is linked or executed at runtime. Native checks adds
a fresh global wrapper even when unused. The C++ compiler itself is not self-hosted.

`passes.py` contains the bootstrap transformations; `stdlib/{sexpr,visitor,ppx}`
now also contain reusable `.on1x` libraries. See [native APIs](../docs/selfhosting.md)
and [the manual](../docs/metaprogramming.md) for
the byte-string protocol, schema, security model, bounds and deferred work.
This is six implementations, not a placeholder catalog of thirty processors.
