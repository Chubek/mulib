# Firestone checked-HIR differential

Build `on1xc` with `-DON1X_WITH_FIRESTONE_HIR=ON -DON1X_WITH_MEMGRAPH=ON`
and build the [MuLL benchmark harness](README.md). Run with a fresh output path:

```sh
build/on1x-benchmark/on1x-benchmark firestone-hir \
  "$PWD/build/firestone-hir/on1xc" /usr/bin/gcc "$PWD/build/firestone-gcc"
```

The benchmark switches `firestone-hir` and `memgraph-bounds` independently
under the debug optimization profile. It compiles the emitted C11 against the
real On1x runtime, then compares stdout, stderr and exit status exactly.
Novostone and the authored optional rewrites are disabled for this measurement.

In the shared fold fixture, Firestone removes checked multiply, subtract and
wide add helpers. The overflow, zero-divisor and dynamic bounds fixtures must
still trap. The [divide-one fixture](novostone-hir/divide-one.on1x) exercises
one million calls to a function dividing its argument by one. With Firestone,
the emitted C no longer calls the checked divide helper in that function, and
the executable must print the same sum. The runner alternates off/on order
and retains five process timings per combination after one warmup for the fold
and divide-one fixtures. Execution timing includes process startup.

The [bounds handoff fixture](novostone-hir/bounds-handoff.on1x) requires
both passes to remove one bounds helper: Firestone shortens the deep checked
arithmetic expression to a literal, then MEMGraph proves its range. The
function with a dynamic index retains its check in all four configurations.

## Verification snapshot

Linux x86-64, GCC 16.2.1 and Clang 22.1.8, application C11 built with `-O2`.
Both compilers passed all 24 configurations in the final run. The same
compiler also passed all 16 configurations in the Novostone benchmark.

| C compiler | MEMGraph | Divide-one loop, Firestone off → on (median) |
| --- | --- | --- |
| GCC | Off | 4.091 → 2.387 ms |
| GCC | On | 4.134 → 2.403 ms |
| Clang | Off | 3.297 → 3.185 ms |
| Clang | On | 3.182 → 3.042 ms |

A preceding run measured 3.965 → 2.327 ms with GCC and 3.212 → 3.109 ms
with Clang with MEMGraph off. The large GCC improvement repeated; the smaller
Clang differences are not a general speedup claim. On the fold fixture, GCC
Firestone emission with MEMGraph off changed from 3.460 to 3.532 ms. Novostone,
run separately on the same fixture, changed from 3.289 to 9.357 ms and did not
fold the wide addition. The passes have different scopes and proof costs.
These are short, unpinned runs; do not generalize them to other workloads.
