# Native stackless timer frames (Linux/glibc AMD64)

The standalone Archon tool can compile a restricted `launch async` /
`await timer` executable. It uses the same checked HIR states and CSP
supervisor as the [direct C11 timer profile](../../docs/async-frames.md).
The resume state dispatch, duration/result computation, slot loads/stores,
peer callbacks, and owner control flow are emitted by the generated Archon
native backend. No generated C source, interpreter, synchronous timer wait
or per-frame fiber is involved.

## Build and run

Configure Archon for `x86_64-unknown-linux-gnu` as in [README.md](README.md).
From the repository root, using `build/on1x-archon` as the build directory:

```sh
b=build/on1x-archon
cmake --build "$b" --target on1x-archon-mir on1x_archon_timer_abi
"$b/backends/mir-archon/on1x-archon-mir" \
  --timer-source on1x/examples/async_timer.on1x --pic -o "$b/timer.s"
cc -c "$b/timer.s" -o "$b/timer.o"
cc -pie "$b/timer.o" "$b/backends/mir-archon/libon1x_archon_timer_abi.a" \
  "$b/libon1x_runtime.a" -pthread -lm -o "$b/timer"
"$b/timer"
```

Expected output is `7`, `42`, and `6` on separate lines. Omit `--pic`
and use `-no-pie` at link time for non-PIC assembly. Installed builds
provide `libon1x_archon_timer_abi.a`; link the installed
`share/on1x/runtime/on1x_runtime.c` in place of the build-tree runtime
archive. The timer ABI archive includes the shared CSP v5 implementation
and one implementation TU for its existing MuLIB kernels. Do not also link
another copy of that scheduler or those kernels into the timer executable.

`--timer-source` accepts a standalone source file with no imports or
extensions and one parameterless `fn main()` containing exactly one
taskgroup (optionally `within`). The group is a flat sequence of:

- Immutable i64 lets and `println` of i64 expressions.
- One or more canonical timer-frame launches, each bound to an affine
  handle. The frame body has the existing ≤128-statement timer profile.
- Optional `launch` peers whose noncapturing i64 body contains only
  immutable i64 lets, i64 `println`, and a final i64 result.
- Direct consuming `join` or `cancel`, their direct `println` forms, or
  binding a successful `join` result to an immutable i64 local.

The group has at most 128 statements, subject to the usual MIR and Archon
compilation budgets. Scalar operands admit i64 literals, earlier i64
bindings, unary `-`, and checked `+`, `-`, `*`, `/`, `%`.
An interrupted join takes the owner’s cleanup edge before its result is
used. Both normal and cancelled exits finish the group through CSP. The
shared supervisor releases each completed or cooperatively cancelled frame
once, including cancellation before first dispatch. Fatal runtime errors
do not unwind.

## Verification and boundary

- `AsyncPlan.cpp` verifies canonical CSP structure (including affine
  ownership, pure initialized i64 slots, and the exact continuation
  sequence) before cloning and rechecking the full checked HIR. The
  admitted scalar operands go through canonical `mir::lower` and
  `mir::verify`; they use the same native bridge and checked arithmetic
  helpers as other Archon scalar code.
- The closed, private timer projection is not serialized MIR. The checked
  states become MachineIR branches and a heap-frame layout of one i64
  state word followed by i64 locals. At a timer the native callback stores
  the next state and returns to the supervisor. Resume re-enters the
  callback on an ordinary native call stack; no stack is retained.
- At pinned LibStone `21f886afe61ce27ef5684acde8f1ed7c064e4e6a`, Archon
  [`Backend.h`](https://github.com/Chubek/libstone/blob/21f886afe61ce27ef5684acde8f1ed7c064e4e6a/cornerstone/Archon/archon/irgen/native/Backend.h)
  provides `FUNC_ADDR`, typed pointer/load/store, branches, and scalar
  function results. It has no aggregate callback return type for the CSP
  `on1x_async_step` struct. `TimerABI.c` adapts a scalar value and written
  kind word into that struct; it neither dispatches states nor schedules
  tasks. Archon's native verifier also checks the emitted MachineIR.
- Public On1x MIR v1–v3 (`include/on1x/backend/mir.hpp`) has no suspend
  edge, frame-local lifetime, task capability, or cancellation effects.
  The SDK `mir-archon` provider advertises only that verified MIR input;
  `on1xc --emit-ir`, the existing SDK-based native driver, and other MIR
  products do **not** gain timer support. They do not fall back to this
  source route. General async functions, dynamic suspension control flow,
  nonblocking task/channel awaits, resources or loans across suspension,
  captures, nested groups, and library/embedding entrypoints are outside
  this profile. A shared effectful MIR schema and independent verifier
  must model frame state, consuming operations, and cancellation cleanup
  before any such capabilities can be transported to MIR products.

With `BUILD_TESTING=ON`, build `on1xc`, `on1x-archon-mir`,
`on1x_archon_timer_abi`, and `on1x_archon_timer_hir_test`. The paired native
and C11 executables use the same runtime and a deterministic monotonic
clock. Tests compare actual resume/drop and fiber-allocation traces,
including peer progress, zero-duration suspension, cancellation before
dispatch and during sleep, deadline drain, and the located negative
duration failure. They also compare the checked division trap on a resumed
step. Forged checked states, types and ownership, unsupported source, output
preservation, and codegen limits are checked separately.

```sh
ctest --test-dir "$b" -R mir_archon_timer --output-on-failure
```

Qualification at On1x baseline `99dd9c3b` used GCC 16.2.1 and the AMD64
backend generated from the pinned LibStone revision above:

- Generated scanner and SDK adapter: `ctest -R '(mir_archon|^on1x.concurrency)'`,
  13 passed.
- Fallback scanner (`ON1X_USE_FALLBACK_SCANNER=ON`), SDK disabled: both timer
  suites passed.
- Clang 22.1.8 ASan/UBSan: the paired native/C11 execution suite passed with
  the timer ABI, CSP runtime, MuLIB implementation TU, and scalar runtime
  instrumented. Leak checking was enabled except for the two deliberately
  non-unwinding fault cases. Archon-emitted assembly itself was not
  sanitizer-instrumented.
