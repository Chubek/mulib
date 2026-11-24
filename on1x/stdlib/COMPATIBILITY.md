# SML Basis compatibility contract

## Meaning of catalog status

| status | meaning |
|---|---|
| `specified_only` | Signature name, requirement class and provenance are present; On1x has no signature language yet. |
| `partial_executable_adapter` | Contains real `.on1x` functions, but is NOT SML signature-complete. |
| `catalog_only` | Name and structural metadata recorded; no working On1x implementation. |
| `requires_functor_language_support` | Functor is inventoried but cannot be encoded as an SML-style module functor yet. |

**The complete inventory is not the complete implementation.** The full
official Basis requires semantics and facilities that current On1x cannot
express faithfully, including generalized parametric datatypes, a robust
exception model, native floating-point and word types, signatures, functors,
streams, and platform-level typed resources. The 2004 manual additionally
distinguishes REQUIRED and OPTIONAL components. Both categories appear in
`CATALOG.json`.

## Core compatibility gaps

| SML area | Partial On1x representation | Missing for compliance |
|---|---|---|
| Bool/General | Concrete primitive functions | `ref`, exceptions, pervasive ML environment |
| INTEGER/WORD | Checked i64, modulo byte arithmetic | Full signed widths, word representation, formatters, scanners |
| REAL/IEEE_REAL/MATH | Host-double decimal-string facade | Native real types, NaNs, infinities, exact rounding/conversion |
| OPTION | Tagged `(bool,T)` tuples | Parameterized datatype, exception-raising `valOf` |
| LIST/LIST_PAIR | Persistent `sexpr` cons | Type-parametric list, complete API, exact exceptions |
| ARRAY | Generation-checked raw `i64` handles | Generic arrays, GC/ownership, SML nested and slice semantics |
| VECTOR | `sexpr` lists with O(n) access | Dense immutable vector with indexed access, generic type |
| STRING/CHAR/BYTE | Byte-level NUL-free APIs | NUL, exact SML character semantics, complete scan/format API |
| TEXT_IO/BIN_IO | Whole-file text and hex-coded binary | Full three-tier stream I/O and configurable devices |
| OS | Selected POSIX-style file/path/process routines | Typed errors, portable OS.IO and process statuses |
| TIME/DATE/TIMER | Timestamp and UTC ISO formatting | Full types, calendars, precision, locale semantics |
| POSIX/UNIX/WINDOWS | Metadata entries | Typed full platform interfaces |
| SOCKET/NET_* | Metadata entries | Network address, descriptors, errors, DNS and safe resource model |
| Functors | Metadata entries | Parameterized SML module/functor semantics |

### ListPair mapping

`std.collections.ListPair` adapts the SML Basis
[`map` and `mapEq`](https://smlfamily.github.io/Basis/list-pair.html) operations
to proper `sexpr` lists. Both functions take `(left, right, f)`, where
`f: fn(sexpr, sexpr) -> sexpr`. They call `f` once per corresponding pair,
left to right, and return a list of its results in the same order.

- `basis_listpair_map` ignores any unmatched suffix. If either list is empty,
  it returns the empty list without calling `f`.
- `basis_listpair_map_eq` returns the empty list for two empty inputs. For
  unequal lengths it calls `f` on the shared prefix, then terminates with
  `ListPair.map_eq unequal lengths`; earlier callback effects are not undone.
  This is a fatal diagnostic, not a catchable SML `UnequalLengths` exception.
- An atom in either root position is rejected before any callback, even if
  the other input is empty. An empty list used *as an element* is valid.

Both functions take linear time and temporary cons storage in the number of
matched pairs (excluding callback costs), subject to the runtime data limits.
The existing `basis_listpair_zip` remains strict for compatibility, unlike
SML `zip`; neither the new mapping operations nor that existing behavior
constitute `LIST_PAIR` signature conformance.

## Design gates for future work

1. Implement ADTs, exhaustive match, modules and signatures before declaring
   Option/List/Vector SML-conforming.
2. Introduce a real typed resource/ownership API for `Array`, streams, files,
   sockets and C handles. Continue to reject unreviewed safe FFI claims.
3. Add IEEE numeric types and native integer/word widths; test against precise
   SML arithmetic including `div/mod` versus `quot/rem` and overflow.
4. Build text and binary streams with lifetime-safe buffering; support NUL
   and byte slices without relying on leaked process-lifetime text snapshots.
5. Create conformance tests for every required signature and optional
   interface that you claim to implement.

See `AGENTS.md` for staged implementation work; do not replace unimplemented
interfaces with empty functions or silently fake SML semantics.
