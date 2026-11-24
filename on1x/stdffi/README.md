# Standard MuLIB FFI

[`mulib/clamp.ffi`](mulib/clamp.ffi) is a portable `ffispec 1` recipe for
`clamp.i64`, `clamp.add_i64`, and `clamp.sub_i64`. The recipe contains bounded,
typed expressions, not C snippets or arbitrary foreign calls. `clamp.i64` checks
`lo <= hi` before executing its body. The saturating arithmetic relies on
short-circuit `and` and untaken `if` branches to avoid evaluating an overflowing
expression. Target and backend selectors apply only to directives, not to the
guard or body. The target-scoped `kernel-hot` hint and `fold-identity` rewrite
are explicitly speculative; the `mir-c11`-only `private-code` hint is not.

## Generate and inspect coverage

From the repository root, with the Cephyr shared library already built:

```sh
export PYTHONPATH="$PWD/on1x/third_party/libstone/cornerstone/Cephyr/python"
export CEPHYR_LIBRARY="$PWD/build/portable-ffi-prefix/lib/libcephyr.so"
generator=on1x/hir/FFIHandler/ffispec-gen.py
python3 "$generator" clamp --output on1x/stdffi/mulib/clamp.ffi
python3 "$generator" clamp --check on1x/stdffi/mulib/clamp.ffi
python3 "$generator" --batch > build/ffispec-coverage.json
python3 on1x/hir/FFIHandler/test_ffispec_gen.py
```

The generator uses `cephyr.Compiler.parse_c11` and `emit_interface` on the
same C11 translation unit: a kernel implementation define and a canonical
header include, with the pinned Cephyr checkout's `Cephyr.toml`,
`--stdlib=cephyr`, `--no-factir`, and `-I include`.
It joins fixed, external, signed-i64 interface declarations to actual AST
definitions using the canonical header path and source line. It lowers only
bounded expressions and `return`/`if` control flow. It rejects unsupported
statements, calls, memory access, casts, unsupported integer conversions,
missing definitions, and disagreement with the review. The interval guard is
from the reviewed API contract, not inferred from the C definition.

`--batch` prints a JSON report ordered by kernel, and writes no `.ffi` files.
By default, it only tries extraction for reviewed profiles. An unreviewed
header reports `no-reviewed-profile`: extraction was **not attempted**, not
shown impossible. A missing reviewed header, unavailable Cephyr, or rejected
reviewed profile is reported with a reason and makes the command exit nonzero.
At this revision the report covers 2,053 headers, of which only `clamp` is admitted.

Opt into a diagnostic projection of unreviewed headers, with repeatable
`--select` for a bounded CI sample. Omit `--select` to project all discovered
headers; that scan is substantially slower and is not part of the default build:

```sh
python3 "$generator" --batch --project --select encoding --select typecore \
  > build/ffispec-projection.json
```

Projection uses both Cephyr views and inventories **external functions declared
or defined in the selected header**, not dependency exports, macros, objects,
or internal/static helpers. It records header lines and signature, body, and
implementation rejection reasons per function, such as pointers, by-value
records, unsupported integer types, or declarations without definitions in
this translation unit. The implementation macro is reported: projection uses
the conventional name if seen, otherwise a sole lexically observed
`MULIB_IMPL_*` in a conditional directive, or the conventional name if several
are seen. It does not request implementations of dependencies.

`shape-compatible` means only that the declaration and body fit the current
bounded i64 syntax. It says nothing about arithmetic safety, API preconditions,
or suitability for execution. Projection never admits an unreviewed export,
writes a recipe, assembles, links, or runs kernel code. A rejected export
does not fail a successfully projected header. A failed extraction instead
reports failed stages and makes the command exit nonzero while continuing with
the other selected headers. For example, `--select texdim` reports missing
`math.h` with the bundled Cephyr standard library; this is not a conclusion
about what another toolchain could extract.

## Independent admission checks

[`reviewed.json`](../hir/FFIHandler/reviewed.json) pins the SHA-256 of the
canonical header and the reviewed guards and bodies, independent of the `.ffi`.
Generation compares the actual header bytes to that pin and the extracted body
to the reviewed expression. Separately, the
[LLSynth recipe compiler](../hir/FFIHandler/README.md) checks the actual header
bytes and the typed recipe's guard and body against the same review before
producing compiler tables. Updating both a header and the SHA claimed in a
recipe does not grant admission. An implementation change needs review of its
types, effects, and arithmetic on every reachable path, not just a new hash.

The notes in [`overrides/mulib/`](overrides/mulib/) document ownership,
lifetimes, callbacks, and failure conditions; they do not grant admission.
