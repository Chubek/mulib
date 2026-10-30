# ebpf

Forward-jump-only register eBPF subset. Instructions: MOV64 K/X (b7/bf), ADD64
K/X (07/0f), JEQ K (15), JA (05), EXIT (95). r0..r9 initially zero, signed
immediates sign-extend; addition wraps uint64. `validate` enforces canonical unused
fields, bounded forward targets, <=65536 instructions and a final EXIT. It returns
1 valid, 0 invalid with optional bad index (SIZE_MAX for non-instruction errors).

`run` returns 0 with r0, -1 invalid, -2 exhausted step budget; output unchanged on
error. Earlier EXIT is allowed. No memory, helpers, packet input, maps, syscalls,
kernel loading or JIT. This is **not Linux's verifier or a security sandbox**.
Public instruction structs are decoded values, not native/wire eight-byte layouts.
Switch: `MULIB_IMPL_EBPF`. Example: `examples/modules/ebpf.c`.
