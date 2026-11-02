# mulib-descritors

The requested filename spelling is preserved. Define `MULIB_IMPL_DESCRITORS`
once before including `mulib-descritors.h`. No dependencies or libc.
[Example](../examples/modules/descritors.c).

All encoders write little-endian hardware bytes into caller-owned buffers.
No packed C structs, global tables or implicit privileged loads are used.
Functions return zero on success and -1 on invalid arguments without modifying
the destination. Buffer sizes in the prototypes are minimum required capacities;
as with ordinary C arrays the caller must supply that much space.

* `segment`: 8-byte descriptor, raw 20-bit limit, byte access flags and 4-bit
  G/D/L/AVL nibble. L+D and L for non-code segments are rejected. Common 64-bit
  kernel code uses access `0x9a`, flags `0xa`; data uses access `0x92`, flags `0xc`.
* `gate`: 16-byte long-mode IDT gate, type 14 interrupt or 15 trap, DPL 0..3,
  IST 0..7. Selector must be a nonzero ring-0 GDT selector. Gate is present.
* `tss`: 16-byte available long-mode TSS descriptor with raw limit 103..0xfffff.
  Caller separately builds the at-least-104-byte TSS, initializes RSP/IST fields,
  and sets its I/O bitmap offset (typically 104 with limit 103 to deny I/O).
* `pointer`: 10-byte GDTR/IDTR operand with limit = byte size - 1, size 1..65536.

`mulib_descritors_tss_init` creates the complete 104-byte long-mode TSS: clears
reserved bytes and RSP1/RSP2, sets RSP0 and seven optional IST pointers, and sets
I/O map offset 104 (use descriptor limit 103). It validates canonicality under
the chosen 48/57-bit mode before modifying output. Zero pointers are permitted
for unused stacks; assign valid mapped stacks before enabling their use.

Caller validates canonical handler/base addresses for the active paging mode,
selector permissions and descriptor semantics. Actual interrupt entry/return
assembly must handle error codes, stack alignment, register saving and red-zone
constraints. `mulib-io.h` optionally loads tables; CS/data/TR reload and interrupt
masking are deliberately separate. Wrong tables can triple-fault a machine.
