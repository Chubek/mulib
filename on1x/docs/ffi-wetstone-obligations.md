# Wetstone numeric obligations for auto-FFI

`hir/FFIHandler/tools/ffi_obligations.py` and
`wetstone_ffi_obligations.cpp` provide an opt-in, host-side analysis helper
for reports. They neither run native calls nor change admission. The reference
revision is LibStone `6a7d52e866db8eb441bb7a4f86ce91253732e740`.

## Generate reports

Build the optional host tool with
`cmake --build BUILD --target on1x-wetstone-ffi-obligations`. Pass its absolute
path, `BUILD/hir/FFIHandler/on1x-wetstone-ffi-obligations`, to
`ffgen --wetstone-obligations`. The generator validates the enriched Cephyr
projection, batches supported checks by module, and records their scope
and statuses in `index.json` alongside the helper's digest. This checks
representable scalar return domains, bounded return policies for the exact
projected definition, and whether an array minimum can fit in `size_t`.
It does not establish that any particular pointer spans that minimum.
The option is independent of `--wetstone-taint` and does not change the
compiled bindings or taint policy. If a selected helper fails, the package
transaction aborts rather than publishing partial subprocess results.

## Integration contract

Decode and validate the same-target Cephyr `interface 1` with
`analysis-model 1` before constructing queries. This helper does not parse C
or the projection. An optional report producer should pass normalized facts
and retain the question, its scope and the result together:

| Question | Inputs and result scope |
| --- | --- |
| Does the declaration's entire integer domain fit a numeric destination? | `RangeCheck(declared_value_range, accepted_range)`. Use the inclusive `value-range`, not an assumed sign or a minimum inferred from the function's name. The result concerns representability, e.g. copying the value of a matching declaration into another integer carrier without loss. It does not establish a normal return, validate a conversion operation, or establish which return values are reachable. |
| Does the exact definition's known return bound fit a numeric destination? | `closed_return_check(declared_value_range, accepted_range, memory_status=..., width_statuses=...)`. Supply the normalized status from the matching `memory-safety` record and the `{8: status, 16: status, 31: status}` from `body-proof` (or `None` if absent). Require `closed-unsigned-expression-v2`, `this-definition`, `satie`, and the matching 32-bit `unsigned int` return type before passing body facts. The helper uses the smallest *proved* width; an absent proof, no proved width, or unproved memory safety leaves the query unknown. This composes Cephyr's proof with a destination bound; it does not re-prove the definition. |
| Could a particular capacity meet a declaration's `static N` minimum? | `ArrayCheck(minimum_elements, element_bytes, capacity_bytes)` for one site and parameter index. Use the constant `requires-array-access` and exact target `sizeof` the *pointed-to* element (e.g. the whole row for `int a[static 4][3]`). The byte capacity is a separate caller-supplied inclusive interval; Cephyr does not provide the actual capacity. A point equal to the target's `SIZE_MAX` asks whether the numeric minimum can even be represented by `size_t`. Keep redeclaration sites separate. Missing capacity, unknown/VLA bounds or unknown layout leave the query unknown. |

Pass query objects (or `None` for an unknown slot) to
`run_obligations(provider, helper_path, checks)`. Add the helper file itself
to the existing `CephyrProvider(tool_inputs=...)` inventory and call
`provider.verify_inputs()` before publishing reports. Results are returned in
input order. A transport error invalidates the entire batch; do not use
partial stdout. An unavailable helper is not a substitute for a positive
result. This API needs no `ffgen`, CLI, loader, or catalogue change to build
and test.

- `proved` for a range query means Wetstone `Interval::leq` and
  `BitvectorInterval::leq` establish containment. Separate negative and
  nonnegative parts support all values from `INT64_MIN` through `UINT64_MAX`
  without narrowing either endpoint. Failure to establish containment is
  `unknown`, not a witness about reachable values.
- For array capacity, Wetstone `BitvectorInterval::div` computes whole
  elements that fit and `less_equal` compares the declared minimum to that
  interval. Division by a positive singleton is exact at the endpoints and
  avoids overflow in `N * sizeof(element)`. `proved` means all capacities in
  the supplied interval are numerically sufficient, `disproved` means none
  are, and `unknown` includes an interval that straddles the minimum.

An array result says nothing about a real pointer's accessible extent,
alignment, overlap, initialization, or lifetime. A body result describes
only the exact analyzed definition, not a linker replacement or an
interposed call. These results are **not** purity, memory-safety, taint,
or security verdicts. Keep Cephyr's original statuses separately; do not
turn an `unknown` solver verdict into a proof, or a numerical result into
permission to load, omit, reorder, sanitize, or safely admit a foreign call.

## Build and qualify

Use an explicitly selected C++20 GCC/Clang host compiler and read-only
LibStone source. No optional Wetstone solver or adapter is required.

```sh
LIBSTONE_ROOT=/path/to/libstone-at-6a7d52e8
CEPHYR=/path/to/shared-build/cephyr
mkdir -p build/wetstone-ffi-obligations
WORK="$PWD/build/wetstone-ffi-obligations"
c++ -std=c++20 -O2 -Wall -Wextra -Werror -isystem "$LIBSTONE_ROOT" \
  on1x/hir/FFIHandler/tools/wetstone_ffi_obligations.cpp \
  -o "$WORK/on1x-wetstone-ffi-obligations"
python3 -B on1x/hir/FFIHandler/tests/qualify_wetstone_obligations.py \
  --cephyr "$CEPHYR" --libstone-root "$LIBSTONE_ROOT" \
  --helper "$WORK/on1x-wetstone-ffi-obligations" --work "$WORK"
```

If wired into an existing CMake build, make the helper an explicit host-only
tool target, with the LibStone root as a system include directory. Do not
link it into the On1x runtime. Native transport has a versioned header
(`wetstone-ffi-obligations 1`), at most 4,096 fixed-size ASCII query rows,
and one verdict per row; the provider also supplies byte, time, and
address-space limits. These limits are not a sandbox.
