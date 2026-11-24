# On1x DomIOS guest core

This optional profile uses checked C11 from the normal On1x frontend and Clang/LLD
to link a freestanding, little-endian x86-64 ELF64 `ET_EXEC`. `_start` at guest
physical address `0x200000` validates the pinned DomIOS v1 boot record via the
guest calling convention, enters C on the DomIOS stack, then sends the exit status
over serial and halts. The output is not a Linux program, has no interpreter or
dynamic dependencies, and is published without execute permission. It must not
be used as a host executable.

The internal stage takes checked generated C, not arbitrary user C:

```sh
python3 on1x/archvirt/compile.py --clang /usr/bin/clang --lld /usr/bin/ld.lld \
  --runtime-include on1x/runtime --archvirt-include "$ARCHVIRT_PREFIX/include" \
  generated.c -o program.elf
on1x-archvirt-run program.elf
```

The stage accepts no user C/compiler/linker flags, uses only the compiler's
freestanding headers and named ABI headers, and does not link a host runtime or
libraries. The core implements checked `i64`, booleans, immutable C-representable
strings and printing, assertions, checked aggregate index/slice/tag traps, and
compiler memory helpers. It does not provide arbitrary externs, floating-point,
managed allocations, general stdlib, Linux process/filesystem/network/FFI, or
concurrency. Missing runtime operations fail compilation or linking. The small
runtime is not a replacement for the full host C11 runtime. Named and
noncapturing callables have null closure environments; captured environments
requiring allocation are not supported.

`on1x-archvirt-run GUEST_ELF` and `on1x_archvirt_run_path()` use exactly the
pinned ArchVirt C API, DomIOS v1, native x86-64 Linux `/dev/kvm` and 40 MiB
guest RAM, with no backend selection from the environment and no software/native
process fallback. The runner checks the shape of a private, sealed ELF snapshot
before asking ArchVirt to load it. No stdin, guest argv or environment, mounted
filesystem, disk, or network is forwarded. A bounded one-MiB capture forwards
serial bytes to stdout/stderr only after execution; guest exit requires a
complete terminal record and no VM error. The native runner uses a five-second
monotonic watchdog (not an instruction count), and reports infrastructure or
protocol errors as exit 125. Runtime assertion/arithmetic/bounds traps use 70,
matching the host runtime for this profile. A program returning 125 is
distinguished from runner errors by the diagnostics. The API is intended for
standalone launchers; pinned KVM reserves `SIGRTMIN+6` inside the host process.

This is not general sandbox certification, validation that an arbitrary guest
ELF is safe, arbitrary Linux-guest support, or an independent review of KVM,
ArchVirt, the host kernel, the compiler, or their dependencies. Only launch
trusted build artifacts using this core.

## Interactive scalar parameters

An On1x source file must explicitly declare the exact guest-only ABI:

```text
extern fn on1x_archvirt_read_i64() -> i64;
fn main() {
  let first = on1x_archvirt_read_i64();
  println(first + on1x_archvirt_read_i64());
}
```

`on1x-archvirt-run --interactive GUEST_ELF` takes an open pipe on stdin. Each
call accepts one ASCII `[+-]?[0-9]{1,19}` followed by LF, with signed-64 range
checked in the guest. Empty lines, partial lines at EOF, non-decimal input
and overflow trap with status 70. Clean EOF at the next read halts with status
0. Successive reads resume the same VM; the caller decides how to retain and
use earlier values. The guest build checks the declaration against its exact
ABI; there is no corresponding symbol in the ordinary host runtime. Batch and
linked launchers still cannot receive host input; attempting the call there
exits with 125.

Guest stdout/stderr are forwarded live without interpreting their contents.
Supply pipes or terminals for those descriptors (`/dev/null` also works), not
blocking regular files. Keep the stdin pipe open across sends, and drain
stdout/stderr to avoid backpressure. Control is limited to scalar parameter
records, EOF and host-side stop; the guest cannot ask to read host files, run
commands, or access the host environment. Closing the input pipe ends the
guest on its next read. SIGINT/SIGTERM stop it even during a blocked read or
output write, returning 130/143. A run is bounded by 5 seconds of active
execution between read waits, 60 seconds waiting for a complete line, 300
seconds of wall time, and 1 MiB of output. The active/idle/wall budgets are
monotonic wall-clock deadlines, not CPU or instruction measurements.

To exercise the core against a built compiler and the pinned ArchVirt install:

```sh
python3 on1x/archvirt/test_core.py --compiler "$ON1XC" --runner "$RUNNER" \
  --clang /usr/bin/clang --runtime-include on1x/runtime \
  --archvirt-include "$ARCHVIRT_PREFIX/include"
```

Pass `--exercise-idle` to wait for the real 60-second KVM serial-read timeout.
