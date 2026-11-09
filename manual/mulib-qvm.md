# mulib-qvm

[Manuals](README.md) · [Library index](index.md)

## NAME AND SYNOPSIS

Quota VM: a portable resumable 16-register unsigned-64-bit interpreter.

```c
#include "mulib-qvm.h"
```

Static-inline dependency requiring only the inline container core. No switch or
external runtime is needed. Example: `examples/modules/qvm-core.c`.

## PROGRAM MODEL

Instructions are C records, not a serialized executable format. Every record
contains opcode, register indices a/b/c (0–15), and u64 immediate. Full validation
checks all records, even unreachable ones, plus absolute jump destinations.

CONST writes immediate to a; MOVE copies b. ADD/SUB/MUL/DIV/AND/OR/XOR/EQ/LT
combine b/c into a. Arithmetic is unsigned modulo 2^64 except division by zero,
which faults. Comparisons produce 0/1. JUMP targets immediate; JNZ branches if
register a is nonzero. HALT uses register a's low eight bits as exit code.

LOAD/STORE access eight little-endian bytes at register b + immediate. LOAD
writes a; STORE reads a. Address addition, bounds and width are checked before
access. HOST invokes the borrowed callback with immediate operation and b/c
values, publishing a only on callback success. YIELD advances PC and returns
AGAIN. Host failures are sticky; host side effects are not rolled back.

## EXECUTION AND OWNERSHIP

`init(vm,code,count,memory_bytes,max_bytes,step_limit,host,user)` deep-copies code
and allocates zeroed RAM, bounding combined code+RAM before allocation. Output
must be uninitialized. Callback/context remain borrowed. Zero step_limit allows
no instructions; memory can be empty if no memory operation executes.

`run(vm,budget)` executes at most budget instructions. AGAIN preserves registers,
RAM and PC. DONE is idempotent after halt. Faults preserve faulting PC/destination
and stay sticky until reset; a faulting instruction counts as a step. Falling off
code, divide-by-zero and lifetime quota exhaustion are explicit errors.

`reset` zeroes registers/RAM, PC, counters, halt/fault state and exit code while
preserving owned code and callback configuration. `free` releases code/RAM and
resets the object. No scheduler, threads or host filesystem access is implicit.

Use [container-qvmboost](mulib-container-qvmboost.md) for lifecycle integration.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-qvm.h`](../include/mulib-qvm.h) |
| Implementation switch | None (support header; see scope above) |
| Repository-local includes | [mulib-container-core](mulib-container-core.md) |
| Complete example | [`examples/modules/qvm-core.c`](../examples/modules/qvm-core.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/qvm-core.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
