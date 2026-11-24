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

To exercise the core against a built compiler and the pinned ArchVirt install:

```sh
python3 on1x/archvirt/test_core.py --compiler "$ON1XC" --runner "$RUNNER" \
  --clang /usr/bin/clang --runtime-include on1x/runtime \
  --archvirt-include "$ARCHVIRT_PREFIX/include"
```
