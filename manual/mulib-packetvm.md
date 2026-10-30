# packetvm — bounded filter interpreter

Define `MULIB_IMPL_PACKETVM`; no dependencies.
See [`examples/modules/packetvm.c`](../examples/modules/packetvm.c).

Programs contain up to 65536 typed instructions and end with RET. The single
32-bit accumulator starts at zero. LD8/LD16/LD32 load absolute packet offsets
in big-endian order; AND masks it; JEQ compares an immediate and chooses a
forward skip from the next instruction; RET returns immediate 0 drop or 1
accept. Unused jump fields must be zero. Both JEQ destinations must remain in
the program, even if one branch is unreachable for a particular packet.

`validate` returns 1/0 and optionally the first invalid instruction index
(SIZE_MAX if no specific instruction is responsible). `run` validates before
executing, decrements an explicit instruction budget, and bounds-checks every
packet load. Return 0 success with verdict, -1 invalid, -2 budget exhausted,
-3 packet too short. Verdict remains unchanged on error. Inputs are borrowed;
there are no allocations, packet writes, helpers, backward loops or syscalls.

This is a small MuLIB instruction set, not classic BPF, eBPF, a JIT, or a kernel
verifier. It is suitable for explicit application packet policy. Tests:
`tests/test_protocol_extensions.c` covers invalid jumps, budgets and short loads.
