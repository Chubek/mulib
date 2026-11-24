# Checked strong `Rc<T>` and `Arc<T>` owners

The direct C11 backend provides `std.container.Rc` and `std.container.Arc`.
Each `own<Rc<T>>` or `own<Arc<T>>` represents one strong claim on immutable
storage. The nominal payload marker is opaque. On1x cannot construct it,
unwrap it with `take`, borrow its storage directly, or manufacture it from an
integer or foreign pointer.

| Operations | Semantics |
|---|---|
| `basis_rc_new(value)`, `basis_arc_new(value)` | Store the value once and return a strong owner. An Rc can also consume an admitted move-only payload via `move`. |
| `basis_rc_clone(owner)`, `basis_arc_clone(owner)` | Borrow one named live owner for the call and produce a separate strong owner of the same allocation. |
| `basis_rc_get(owner)`, `basis_arc_get(owner)` | Borrow one named live owner and return a copy of its payload; managed `sexpr` values are independently retained. Move-only payloads cannot be read this way. |
| `basis_arc_parallel_count(owner, byte, workers)` | On an `Arc<array<u8,N>>` for `1 <= N <= 256`, return the number of byte matches. Accepts 1–16 requested workers; invalid worker counts or native startup failure return `-1`. All started workers are joined before returning. |

```on1x
import std.container.Rc as R;
import std.container.Arc as A;

fn main() {
    let a = R.basis_rc_new(sx_string("shared"));
    let b = R.basis_rc_clone(a);
    drop(a);
    assert(sx_text(R.basis_rc_get(b)) == "shared");

    let bytes = A.basis_arc_new(array[255u8, 0u8, 0u8, 1u8]);
    assert(A.basis_arc_parallel_count(bytes, 0u8, 4) == 2);
}
```

These operations are direct-call compiler intrinsics exposed by the standard
modules, not raw `extern`s or first-class function values. Pass the owner by
`move` when a function is to consume it. A lexical drop, `drop(owner)`,
early return, or mutable overwrite releases that claim exactly once. The last
claim destroys the shared allocation and its payload. Construction allocates
the payload block and a single ownership box; a clone increments the same
payload's strong count and allocates only a fresh ownership box. Failure to
allocate either box or to increment a saturated count uses the existing
owning-allocation emergency cleanup and exits with status 72. A failed clone
does not leave an extra claim behind.

Supported payloads are bounded checked scalar and aggregate values. Rc also
accepts managed `sexpr` and finite unique owners where admitted by the
existing layout checker. Arc accepts only pointer-free immutable values and
never installs a payload destructor. Ordinary `string`, dynamic `buffer`,
closures, JSON, and foreign handles are excluded.

## Native workers are a closed ABI boundary

Arc's strong count uses MuLIB `xatomics` with sequentially consistent
operations. The bounded count operation gives each native `xthreads` worker
its own strong claim before publication. The worker reads only its immutable
byte range, returns its result via join, and releases that claim. Partial
startup releases the unstarted worker's claim and joins the started workers.
Neither the On1x owner box nor the process-local On1x cleanup registry is
accessed by workers. No On1x callback or C pointer enters this API; there is
no detached work or thread handle exposed to On1x.

This does **not** establish general On1x `Send`/`Sync`. Rc/Arc owners still
obey the finite-owner restrictions on captures and raw externs. The current
cooperative runtime rejects compilation units mixing its task/channel model
with finite owners. Native operations cannot be generalized to arbitrary
On1x closures or types without checked effect, capture, handoff, and cleanup
semantics. Rc/Arc have no weak references, cycle collector, mutable payloads,
or scalar MIR representation.

## Dynamic, local `RefCell<T>` guards

`std.container.RefCell` uses the same lexical ownership machinery with a
single-threaded, mutable cell. `basis_refcell_new(value)` returns
`own<RefCell<T>>`, and `basis_refcell_clone(cell)` creates another cell owner
of the same storage. Acquire a guard against that storage through any alias:

- `basis_refcell_try_borrow(cell)` returns `RefCellBorrow<own<Ref<T>>>`.
- `basis_refcell_try_borrow_mut(cell)` returns `RefCellBorrow<own<RefMut<T>>>`.
- Both return `Busy` on conflict or `Granted(guard)` on success. Multiple
  readers may coexist; a writer excludes both readers and other writers.
- `basis_refcell_read(reader)` and `basis_refcell_read_mut(writer)` copy a
  snapshot. `basis_refcell_replace(writer, value)` transfers a replacement
  into the cell and returns the previous value. Only the writer can mutate.
  Compute a replacement from the same writer before calling `replace`, which
  borrows it exclusively while its arguments are evaluated.

```on1x
import std.container.RefCell as C;

fn main() {
    let cell = C.basis_refcell_new(41);
    let alias = C.basis_refcell_clone(cell);
    match C.basis_refcell_try_borrow_mut(alias) with {
        Busy() => assert(false),
        Granted(writer) => assert(C.basis_refcell_replace(writer, 42) == 41)
    };
    match C.basis_refcell_try_borrow(cell) with {
        Busy() => assert(false),
        Granted(reader) => assert(C.basis_refcell_read(reader) == 42)
    };
}
```

Each guard is move-only, roots the cell independently, and releases its
dynamic loan on drop or lexical cleanup. A guard may outlive all cell owners;
it cannot be cloned, unboxed, borrowed directly, or converted to a pointer.
There are no runtime callbacks or escaped references. Payloads are bounded,
copyable values (including managed `sexpr`); holding a finite owner inside a
RefCell is not admitted, avoiding general mutable ownership cycles. A failed
guard allocation uses the existing terminal allocation cleanup rather than
returning the unrelated `Busy` result. RefCell and its guards remain local
and cannot participate in Arc's worker operation.

`on1x-build.py` selects `container/shared.c`, MuLIB `xatomics` and `xthreads`
with one implementation TU, and `-pthread` on POSIX. For example:

```sh
python3 on1x/tools/on1x-build.py app.on1x --on1xc PATH/TO/on1xc -o app
```

`on1x_container_shared.h` is a private generated C11 ABI, not a C embedding
contract for forged or stale pointers.

## Tests

```sh
python3 on1x/tests/test_container_shared.py PATH/TO/on1xc on1x /tmp/opencode/shared-gcc gcc --cflag=-O2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 on1x/tests/test_container_shared.py PATH/TO/on1xc on1x /tmp/opencode/shared-san clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wno-error=tsan \
  -D_POSIX_C_SOURCE=200809L -pthread -fsanitize=thread -no-pie \
  -I kernels -I on1x/runtime -I on1x/stdlib/container \
  on1x/tests/test_container_shared_stress.c on1x/runtime/on1x_runtime.c \
  on1x/tests/fixtures/container_shared/kernels.c -o /tmp/opencode/shared-tsan
/tmp/opencode/shared-tsan
```

The suite checks generated C11 lifecycle, managed and unique payload cleanup,
26 allocation faults, four partial startup faults, source ownership
rejections, and dynamic RefCell conflict/guard lifetimes. It also verifies
link-plan deduplication and fail-closed scalar MIR. The C++ verifier test
mutates canonical HIR to check rejection of forged claim and loan metadata.
A separate native test races retains/releases and verifies exactly one final
free per allocation after the main thread relinquishes its claim. GCC may
warn that TSan cannot instrument the MuLIB fence helper; the shared containers
use load/CAS/fetch-sub and do not call that helper.

Validated on x86-64 Linux, MuLIB base `c2cbbf70537d`, LibStone
`3399047f6c63`, with GCC 16.2.1 (generated scanner) and Clang 22.1.8
(fallback scanner). Strict C11 at `-O2` and ASan/UBSan with leak detection
passed on both compilers (`-no-pie` added to GCC sanitizer runs); the native
strong-claim stress passed GCC TSan. These are not general race-freedom
results for On1x applications.
