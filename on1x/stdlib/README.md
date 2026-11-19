# On1x Basis — SML-inspired library and complete SML Basis inventory

This is On1x's standard-library source tree, loaded by the compiler's
[module system](../docs/modules.md).
It is **not a conforming implementation of the Standard ML Basis Library**.

## What is included

- `CATALOG.json` and `catalog/{signatures,structures,substructures,functors}`:
  all 40 required signatures, 43 required structures, 24 optional signatures,
  64 optional structures, 3 optional functors, and 12 nested structures listed
  in the official SML Basis overview. Optional fixed-width families (`Int<N>`,
  `Word<N>`, `Real<N>`, `Pack*<N>`) are represented as templates and include
  typical concrete width examples. These are API inventory metadata, **not
  executable implementations**.
- 60 structure/substructure entries currently point to **partial executable
  adapters**. Source counts are generated in `CATALOG_SUMMARY.json`.
  Even a file named like an SML module implements only a documented subset.
- `runtime/basis_native.[ch]` and `runtime/basis_real.c`: C11 adapter for
  checked mutable `i64` arrays, file operations, binary-to-hex I/O, monotonic
  clock, UTC date formatting and decimal-string `double` computations.
- `tests/` includes native runtime tests, static checks, and On1x smoke examples.
- `base/`, `batteries/`, `graph/`, `parsec/` and data/transport extension modules
  add persistent S-expression helpers and canonical MuLIB scalar adapters.
  [`_bridge/README.md`](_bridge/README.md) describes automatic linkage, recoverable
  status, bounds, binary sockets and the explicitly raw trust boundary.

## Architectural differences

Current On1x does not yet have all of SML's ADTs, structures, signatures,
functors, typed exceptions, polymorphic arrays, native unsigned machine words,
and native floating-point types. It compiles to C11 and currently has `i64`,
`bool`, NUL-free `string`, managed `sexpr` and `buffer`, closures, and tuples.

Accordingly:

- Module names map to `.on1x` files with isolated compile-time namespaces.
  Implementations retain `basis_...` names; small facade modules re-export short names.
- List/Vector are persistent `sexpr` lists, not general SML polymorphic lists
  or random-access vectors. `Vector.sub` is O(n).
- The explicit `std.Option` compatibility facade uses a `(bool,payload)` pair,
  not the pervasive nominal `Option<T>`.
- Array is a mutable fixed-size `i64` **handle**, not a polymorphic array;
  it must be explicitly freed, and the compiler cannot prove ownership.
- String/Char are byte-oriented, cannot represent embedded NUL, and are not
  guaranteed to satisfy SML's exact character and string semantics.
- Binary I/O uses hex to preserve arbitrary bytes over the current string ABI.
- Real/Math use decimal-string-wrapped host `double`. No faithful IEEEReal,
  NaN, infinity, rounding-mode or precision semantics are claimed.
- I/O, POSIX, Windows, sockets, complete Date, advanced real/word/functor
  APIs and other missing components are inventoried but NOT implemented.
- The C11 adapter is an unsafe raw FFI boundary, NOT a proven ExoML/Chisel
  contract or a full multi-platform SML Basis implementation.

## Directory map

```text
on1x/stdlib/
  pervasive.on1x     default functions plus nominal Option/Result helpers
  Int.on1x, ...     short-name Int/String/List/Option/Functional facades
  core/              General Bool
  numeric/           Int, Word8, Real/Math bridges, widths, IntInf natural adapter
  collections/       Option, List, ListPair, Array, Vector and mono adapters
  text/              String, Char, Substring, StringCvt, Text, Byte
  io/                TextIO, BinIO and primitive facade
  os/                OS.Path, OS.FileSys, OS.Process, OS.IO, CommandLine
  time/              Time, Timer, Date
  extensions/        Result, Functional, Assert (NOT SML Basis)
  runtime/           C11 native bridge
  catalog/           per-component compatibility descriptions
  tests/             source examples and native validation
```

## Quick usage

```on1x
import std.Int as Int;
from std import String;
fn main() {
  assert(Int.div(-7, 3) == -3);
  assert(String.contains("abracadabra", "cada"));
  assert(identity(42) == 42);
  println("ok");
}
```

Quoted imports remain supported and resolve relative to their source files.
`stdlib/sexpr`, `stdlib/visitor`, and `stdlib/ppx` provide the native authoring APIs.
`stdlib/diagnostics` provides bounded diagnostic values and descriptor-3 PPX
registration/emission helpers; the compiler still validates every ID against
the selected declarative catalog.

## Expanded APIs and corrected semantics

- `Int.same_sign` distinguishes zero from positive/negative values; `rem` and
  `mod` return zero for `INT64_MIN, -1`, while division still detects overflow.
- `String.find(text, needle, start)` returns a byte index or `-1`.
  Prefix/suffix/contains/find compare bytes without allocating search snapshots.
  `String.join(separator, strings)` joins a proper list of `sx_string` atoms.
  Substring bounds are validated before addition.
- `List.tabulate` calls its function in ascending index order. `app`, `foldr`
  and stable `partition` complement the existing map/filter/foldl operations.
  Lists contain `sexpr` elements; malformed root atoms are rejected.
- The automatic pervasive environment provides bounded nominal `Option<T>` and
  `Result<T,E>` plus `option_*`/`result_*` constructors, predicates, transforms,
  fallbacks, conversions and focused iteration helpers. The helpers retain their
  copy-safe payload profile; direct `Result ?` separately supports checked shared
  and finite-own payload transfer. `Option ?` is not supported.
- Legacy `std.Option.some(value)`, `none(default)`, `is_some`, `get_or`,
  `map(option,f,default)` and `and_then(option,f,default)` continue to work
  polymorphically over tagged tuples. `none` retains its typed default payload.
- `Functional.compose(f,g)`, `pipe(value,f)`, `flip(f)`, `curry(f)` and `uncurry(f)`
  are polymorphic closure-based operations. `compose(f,g)(x)` means `f(g(x))`.
- Native snapshot budgets include allocation overhead; binary-read allocation
  failure releases its temporary buffer. `BASIS_MALLOC` permits failure injection.

Run the integrated tests with `ctest --test-dir BUILD -R 'libraries|modules'`.
`catalog/refresh.py` refreshes relocated paths, counts and file hashes while
preserving every compatibility status.

## New extension library boundaries

- `base` list/sequence/string helpers reuse checked Basis implementations;
  searches and `parsec_literal` compare bytes without per-position snapshots.
- S-expression Option/Result encodings validate tags and arity. Their new
  `basis_base_option_map`, `basis_result_map` and `basis_result_and_then` preserve
  error/None paths without invoking callbacks. These are not compiler-native ADTs.
- Association maps validate pairs/string keys and offer `basis_map_find` and
  `basis_map_remove`. Removal removes all shadowed bindings for the key.
- Graphs validate two-string edges; BFS marks vertices when queued, preserves
  adjacency order, deduplicates cycles/parallel edges, and bounds edge scans plus
  visited-membership work. Malformed graph inputs reject rather than truncate.
- `basis_parsec_int` scans signed decimal i64 including `INT64_MIN`, returning
  `(success,value,end)`; malformed/overflowing input rolls back to the start.
  Whitespace scanners reject positions outside the input.
- JSON owners/views remain lexical compiler intrinsics. `json/json.on1x` exposes
  scalar operations instead of unsupported resource parameter/return signatures.
- Legacy extension names are retained for compatibility; newly added interfaces
  use unique `basis_*` names. `stdlib/ppx`, `sexpr` and `visitor` are preserved.

`on1x.stdlib_extensions` and the two additional pure-library smoke programs are
part of CTest. Commands and verification results are in [VERIFICATION.md](VERIFICATION.md).

See [INSTALL.md](INSTALL.md), [COMPATIBILITY.md](COMPATIBILITY.md),
[AGENTS.md](AGENTS.md), and [CATALOG.json](CATALOG.json).

## Specification

The identifier inventory is based on the SML Basis official overview:
https://smlfamily.github.io/Basis/overview.html

Individual semantics and signature definitions must be consulted at:
https://smlfamily.github.io/Basis/manpages.html

The official manual's text is not reproduced here. This package is an
original, experimental On1x adaptation. Do not advertise SML compliance
until each required interface passes a conformance test suite.
