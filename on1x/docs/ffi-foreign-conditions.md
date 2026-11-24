# Explicit foreign status conditions

`on1x/hir/FFIHandler/tools/ffi_conditions.py` generates an ordinary On1x
module that turns a **returned integer status** into a nominal condition.
The native call still goes through its existing raw or independently admitted
boundary. The classifier neither calls nor authorizes C:

```on1x
from std.core.Pervasive import Result;
import CephyrCondition as Condition;

# The caller must separately validate, link and own this native bridge.
extern fn fixture_projection_status(mode: i64) -> i64;

fn project(mode: i64) -> Result<i64, Condition.ForeignCondition> {
    let status = Condition.signal_status(fixture_projection_status(mode))?;
    return variant<Result<i64, Condition.ForeignCondition>>(Ok, status);
}
```

Each named failure produces `Err` with its original signed status as the
case payload. Only the explicitly listed success value produces `Ok`.
Any other number produces `Err(UnrecognizedStatus(number))`: the number
violated the contract, but the tool does not label it a known provider
failure. Use ordinary `match` to recover or `?` to signal it to the caller.
The latter returns from the current function and runs normal `defer` and
ownership cleanup, with the same nominal `Result` and error identity rules
as any other On1x code. It is **not** a throw through C, an asynchronous
signal, a general exception handler, or a catcher for traps or `longjmp`.
Neither the On1x runtime nor the native ABI changes.

## What the pinned Cephyr actually reports

At LibStone `e10ac69eed7baf91b06d15c4e0d58360097cb5c4`,
[`Cephyr/docs/C11Interface.md`](../third_party/libstone/cornerstone/Cephyr/docs/C11Interface.md)
and `Cephyr/src/interface.cpp` describe and implement `--emit-interface=sexp`
and `cephyr_emit_interface`. They provide composite C types, linkage,
calling convention, specifiers (including written `_Noreturn`), and enum
constants. The declaration stage does not lower function bodies. The other
S-expression form (`--emit-ast=sexp` / `cephyr_parse_c11`) is a syntax
document, not verified control flow or effect information.

Neither wire format relates an `int` return to an enum, identifies its
success/failure values, guarantees normal return, reports when `errno` or
out slots are valid, classifies `NULL`, or establishes unwind/cleanup
compatibility. A caller needs the exact implementation and contract to make
those decisions. In particular, `_Noreturn` does not mean “throws.”

## Version 1 authored contract

The generator takes the original C11Interface bytes and a bounded JSON
document. The
[Cephyr SDK fixture](../hir/FFIHandler/tests/foreign_conditions/cephyr_status.json)
is an example. The exact fields are:

| Field | Meaning |
| --- | --- |
| `version` | Integer `1`. |
| `interface_sha256` | SHA-256 of the exact projection bytes. Regenerate and review the contract when the projection changes. |
| `symbol` | Exact externally linked, fixed, prototyped SysV function returning C `int`. The profile excludes `inline` and `_Noreturn` specifiers. |
| `enum` | Unique complete named C enum tag declaring the status constants. |
| `success` | One enumerator in that tag whose return denotes success. |
| `failures` | 1–16 enumerator names in that tag mapped to distinct On1x case names, each beginning with an ASCII uppercase letter, then ASCII letters or digits. `UnrecognizedStatus` is reserved. |

Overlapping mapped values (including aliases), missing enumerators, wrong
targets, mismatched digests, unknown fields and malformed documents are
rejected. Generation is currently restricted to the AMD64 Linux C11Interface
layout. The generator uses the existing FFIHandler reader; it doesn't parse C
or inspect AST bodies. It writes only On1x source to stdout on success:

```sh
python3 -B on1x/hir/FFIHandler/tools/ffi_conditions.py \
  status.interface status-contract.json > MyCondition.on1x
```

The digest binds the mapping to a declaration snapshot, not its origin,
reviewer, or linked implementation. The mapping and its association with the
function are explicit assertions by the contract author. Generate one
module per mapping and use it only with the corresponding separately
validated native call. The compiler checks the condition and its propagation,
not where an arbitrary `i64` given to `signal_status` came from. This tool
does not publish a descriptor, change the generated catalogue or import
policy, or make an unsafe call safe.

## Native qualification

The bounded example uses `cephyr_emit_interface`, whose return cases and
result ownership are documented in `Cephyr/include/cephyr/Cephyr.h` and
implemented in `Cephyr/src/main.cpp` (`drive` and `compile_units`). Its
test-only C bridge validates the prototype, calls the actual public SDK, and
destroys the `cephyr_result` using Cephyr's C API. Pointers and `NULL` are
handled at that existing C boundary, not inferred from the S-expression.
The bridge exposes only `int64_t` to On1x. The test uses the SDK status, not
the CLI exit code (which collapses distinct failures).

With an On1x build and a pinned Cephyr CLI/embedding library, from the repo root:

```sh
python3 -B on1x/hir/FFIHandler/tests/test_ffi_conditions.py \
  --on1xc "$ON1X_BUILD/on1xc" \
  --cephyr "$CEPHYR_BUILD/cephyr" --libcephyr "$CEPHYR_BUILD/libcephyr.so" \
  --libstone-root "$LIBSTONE" --work "$SCRATCH/foreign-conditions"
```

The test exercises real projection success, a parse failure, and a rejected
SDK request, each with one native invocation, LIFO defers, and owner
cleanup. It separately checks the generated `InternalError` and
`UnrecognizedStatus` arms with inputs to the classifier alone, without
claiming to induce an internal SDK failure. It also checks malformed
contracts/projections, nonadmissible declarations, and incompatible nominal
conditions. This qualifies the reference On1x C11 path, not projected
`errno`, pointer/ownership layouts, callbacks, C unwinding, or a new MIR
effect.
