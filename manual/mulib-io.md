# mulib-io

Define `MULIB_IMPL_IO` once. By default every hardware operation returns -1,
making hosted inclusion safe. For GCC/Clang x86 ring-zero code define
`MULIB_IO_ENABLE_PRIVILEGED` **before the implementation include**.
No other switches are needed. [Example](../examples/modules/io.c).

`mulib_io_in8`/`out8` perform byte port accesses. `in8` requires nonnull output.
`mulib_io_load_table` loads a 10-byte descriptor operand with LGDT (argument 0)
or LIDT (argument 1), only on x86-64. It does not reload CS, data registers or TR.
All functions return zero when executed or -1 for unavailable/invalid operations.
The port/table memory is caller-owned. There is no allocation or OS dependency.

Native instructions carry compiler memory clobbers, but do not replace device
timing, MMIO barriers or platform-specific synchronization. Opt-in is a build
gate, **not** a runtime privilege check. Enabling it in a hosted process may fault.
Interrupt state, table validity, CPU feature support and safe handoff are entirely
the caller's responsibility. Never execute the privileged example path under a
normal user-mode test runner. Other compilers/architectures return unavailable.

`mulib_io_in32`/`out32` provide actual dword port I/O under the same privileged
opt-in. They support CF8/CFC configuration access; the application must serialize
the address/data pair across CPUs and interrupts. Null input-output pointers are
rejected. Default hosted builds return -1 without touching hardware.
