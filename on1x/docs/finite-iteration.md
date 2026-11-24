# Finite snapshot iteration

```on1x
let rows = array[(1,(10,99)),(2,(20,99)),(3,(30,99))];
total: for (index,(points,_)) in rows[1:3] {
  if index == 2 { continue total; }
  println(points);
}
```

`for pattern in operand { ... }` is a statement. The operand must have
a known fixed array or snapshot slice type (`array<T,N>` or `slice<T,N>`).
Annotate a parameter if its shape is otherwise unknown. An empty array
literal still needs enough ordinary element-type context.

The operand evaluates once, before any pattern names exist. The loop retains
a value snapshot, reads its logical length once, and visits indices in
increasing order. Replacing the source variable during the body does not
change that snapshot. A sliced value keeps its composed offset and runtime
length; backing capacity is not the number of iterations. An empty array or
slice still evaluates the operand and then skips the body. A direct `borrow`
expression is not an iteration operand; this feature does not introduce a
loan or an element reference. Reading an already-borrowed named array/slice
as a value takes a copy under the existing borrow rules.

Patterns are irrefutable: a name, `_`, or a possibly nested tuple of these
patterns. Every named leaf is a fresh immutable local, visible only in the
iteration body. `_` introduces no source-visible name. Duplicate names and
tuple type/arity mismatches reject even on an empty value. A block may shadow
a binder as usual. Historical `for name in lo..hi`/`lo..=hi` remains separate:
its single identifier (including `_`) is still a binding, not a wildcard.

`continue` takes the target latch, advancing one index after lexical cleanup.
`break` skips that latch. A labeled exit uses the same rules across nested
loops; `return` uses normal function cleanup. Existing fatal trap and
function-scoped `defer` semantics do not change.

## Lowering and backend limits

The parser expands finite iteration into existing source `Scope`, `Let`,
`ControlWhile`, index and length operations. Macro hygiene alpha-renames
internal names before Unit-v1 PPX transport. Inference and semantic checking
restrict the preheader length operation to arrays/slices and reject a borrowed
alias; successful checked HIR contains only the ordinary data operations and
a verified loop target/latch. No scanner tokens, ASDL constructors, MIR
opcodes or runtime ABI are added.

The direct C11 backend and the VMWeave product lowering execute this profile
subject to their existing type, layout, and body restrictions. VMWeave does
not inherit C11's foreign/resource/cleanup support merely by admitting this
loop. Embedded Cephyr also executes loops over local aggregates on AMD64;
it still rejects by-value aggregate function ABI, such as the array return in
`examples/finite_iteration.on1x`. The ordinary scalar MIR evaluator,
MIR-to-C11 route and the ordinary source-to-Archon route do not admit the
required array/slice layouts and do not silently fall back.
See [VMWeave](../backends/mir-vm/README.md).

Arrays and slices are the only admitted operands. The stdlib compatibility
lists use `sexpr`, and a user-defined recursive `own<List>` has no intrinsic
iteration semantics. Regex captures are not iterable either, even though
they support indexing separately. Intrinsic typed lists, cons/tail and list
patterns, constructor/record/array or guarded/refutable iteration patterns,
filtering/comprehensions, and general iterator/drop protocols are not
implemented by this feature.

## Verification

Run `examples/finite_iteration.on1x` for `100`, `30`, and `9`. With
`ON1X_LIBSTONE_ROOT` pointing to LibStone
`3be5e9d7ffde4f97f333365e7080e316396a0e55` (read-only in the worker run):

```sh
mkdir -p build/tmp
export TMPDIR="$PWD/build/tmp"
cmake -S on1x -B build/iter-generated -DBUILD_TESTING=ON
cmake --build build/iter-generated --parallel 3
# Unix Makefiles with CMake 4.4 may require install-time relink artifacts.
cmake --build build/iter-generated --target preinstall --parallel 3
ctest --test-dir build/iter-generated --parallel 3 --output-on-failure

cmake -S on1x -B build/iter-fallback-clang -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-sanitize-recover=all;-fno-omit-frame-pointer'
cmake --build build/iter-fallback-clang --parallel 3
ctest --test-dir build/iter-fallback-clang --parallel 3 --output-on-failure \
  -R '^on1x\.(control(_hir)?|pattern(s|_hir)?|value_loops?(_hir)?|vm_data|vm\.(opcodes|source)|acceptance\.slice_snapshots)$'
```

`on1x.control` and `on1x.control_hir` cover direct C11, verified standalone VM
artifacts with and without optimization, source and HIR rejection, identity
PPX, staging, and C11 lifetime accounting. The sanitizer flags instrument
the generated C11 programs and their linked C runtime, not the compiler or VM.

Worker baseline: `825f65857db0b4c03aee860e297a7d1f33401e77`; implementation
and fixtures: `660cd130`. CMake 4.4.3 with GCC 16.2.1 and the generated scanner
passed the iterator, scanner parity and VM tests in the 216-test run.
The initial run had 208 passes, seven failures and one skip. All six install
failures passed after `preinstall` and
`ctest --test-dir build/iter-generated --rerun-failed --output-on-failure --parallel 3`.
The remaining failure was the relocated `on1x.processor.snapshots` check
already recorded in [`COMPLETION_REPORT.md`](../COMPLETION_REPORT.md).
`on1x.container_pod_cgroup` skipped (no delegated cgroup). Logs:
`build/iter-generated/full-tests.log` and `build/iter-generated/recheck-failed.log`.
Clang 22.1.8 plus the fallback scanner and generated-program ASan/UBSan
passed all 10 focused tests (`build/iter-fallback-clang/tests.log`).

The optional AMD64 test used existing installed Cephyr and Archon packages:

```sh
cmake -S on1x -B build/iter-native -DBUILD_TESTING=ON \
  -DON1X_WITH_CEPHYR=ON -DCephyr_DIR:PATH="$Cephyr_DIR" \
  -DON1X_BUILD_MIR_ARCHON=ON -DArchonBackend_DIR:PATH="$ArchonBackend_DIR" \
  -DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON \
  -DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu
cmake --build build/iter-native --parallel 3 --target on1xc
ctest --test-dir build/iter-native --output-on-failure --parallel 3 \
  -R '^on1x\.cephyr\.(native|options)$'
build/iter-native/on1xc --sdk-emit mir-archon \
  --sdk-target x86_64-unknown-linux-gnu on1x/examples/finite_iteration.on1x \
  -o build/iter-native/finite-archon.s
```

Both Cephyr tests passed, including link/execute of the labeled tuple-pattern
loop over a local slice with base, SandSLAB and Blackstone objects. The
Archon command fails with `ON1X-MIR-UNSUPPORTED: scalar MIR cannot lower array`
and leaves a pre-existing output file untouched. The Cephyr example with an
array-returning function fails with `cephyr.abi.aggregate` and also preserves
the existing output. Logs: `build/iter-native/tests.log` and
`build/iter-native/iterator-results.log`.
