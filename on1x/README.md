# On1x Standard Library + native PPX extension batch

These extensions are integrated with the existing C11 compiler and module loader.
The refined Basis modules, `stdlib/ppx/`, `stdlib/sexpr/`, `stdlib/visitor/`,
original native processors and contracts are preserved.

## Language implementation continuation

The M1 numeric value slice adds real `f64` to the existing HIR/C11 compiler:
arithmetic/comparisons, explicit checked conversions, exact bit transport and hex
printing, closures/tuples/staging, and raw C `double` leaves/results. The initial
profile is Linux x86_64 GCC/Clang. Decimal literals and MIR v2 now execute floats
and nested trivial tuples, with checked serialization, an evaluator and C11
emission; legacy scalar MIR remains v1. A further M1 slice adds the bounded value
data model: structural records, closed tagged sums with exhaustive `match`,
fixed arrays and owning snapshot slices — all by-value C11 with bounds-checked
access and terminal located value errors (`examples/data_model.on1x`). General
ownership, safe FFI and concurrency are still not admitted. See the
[numeric contract](docs/numerics.md), [data-model contract](docs/data-model.md),
[capability matrix](docs/capability-matrix.md) and
[verification record](VERIFICATION.md). This does not complete M1 or On1x.

The repository accessed through the GitHub connection exposed the prior PPX-v1 schema and Exotract/ExoML contract catalogs. The `on1x/std/ppx` path in the request appears to refer to the actual `on1x/stdlib/ppx` path, whose `ppx.on1x` is used by all newly added native PPXs.

## Contents

- Source-only `.on1x` facades: JSON, YAML, TOML, INI, Parsec, crypto, Unix, sockets, network, IPC, NNG-style envelopes, RPC JSON request construction, Shexp ASTs, hashconsing, graph traversal, Base and Batteries-like structures.
- `_bridge/mulib_bridge.c`: raw C ABI bridge against *real MuLIB public headers* and an independently built Exotract one-TU `libmulib.a`. C bridge is **not** safe by ExoML/Chisel criteria. It requires reviewed extraction and independent memory/effect checks before a safe FFI claim.
- Fifteen native On1x PPXs: the original eight plus assertions, cond, format,
  logging, state_machine, tap and trace, with Python/native execution comparisons.
- [`stdlib/CATALOG.json`](stdlib/CATALOG.json): complete named SML Basis inventory;
  extension modules remain partial adapters, not Base/Batteries API parity.

## Build (C11 adapter)

The build helper selects bridge/kernel groups automatically and preserves one
kernel implementation TU. Alternatively supply a locked Exotract archive through
`--mulib-archive DIRECTORY`; do not define kernel switches in consumer adapters.

```sh
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON
cmake --build build/on1x --parallel 4
python3 on1x/tools/on1x-build.py on1x/examples/stdlib_extensions.on1x \
  --on1xc build/on1x/on1xc -o build/stdlib-example
build/stdlib-example
ctest --test-dir build/on1x --output-on-failure
```

Tests execute canonical MuLIB JSON/YAML/TOML/INI, cryptohash, numeric-IPv4 sockets,
pure helpers and actual native PPXs. See [VERIFICATION.md](VERIFICATION.md) for
the measured GCC/Clang/sanitizer configurations and original historical reports.

## Status caveats

These are **not** complete Jane Street Base, Batteries or SML Basis ports.
`shexp` does not execute processes, `nng` is not libnng, IPC is an in-process
mailbox helper and LibStone interchange does not wrap C++ APIs. Scalar text is
NUL-free; socket hex reception preserves binary bytes. Tuple-returning wrappers
distinguish missing/error/EOF/empty outcomes, but remain trusted raw adapters.
Generated f64 bindings, record/slice ABI and ExoML/Chisel safe-import gates remain
open; the new user-declared raw `double` ABI is not safe foreign admission.
Read [the bridge manual](stdlib/_bridge/README.md), [PPX manual](stdppx/README.md)
and `AGENTS.md` before production use.
## Error reporting and repair proposals

Optional Syren integration (`-DON1X_WITH_SYREN=ON`) adds text/JSON/SARIF source
diagnostics and `on1xc --repair INPUT -o NEW_FILE`. Repairs are independently
compiler-checked proposals, never in-place edits or changes to ON1X's syntax or
semantics. See [diagnostics and repair](docs/diagnostics.md).
