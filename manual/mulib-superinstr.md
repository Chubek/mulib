# mulib-superinstr

Exact repeated assembly sequences factored into GAS macros. All implementation
code lives in `include/mulib-superinstr.h`.

Define `MULIB_IMPL_MIR`, `MULIB_IMPL_STRBUF` and `MULIB_IMPL_SUPERINSTR` before
including the header in one translation unit. Link `-lm` for MIR.

`mulib_superinstr_pack(text, length, symbol, options, &result)` identifies exact,
nonoverlapping repeated instruction-line sequences and returns owned text.
Definitions precede the original assembly, occurrences become invocations, and
`.purgem` directives release temporary macro names afterward. `symbol` namespaces
names and must satisfy MIR's 1..127-byte ASCII identifier rules.

This is the macro realization of superinstructions: expansion preserves the
original instruction stream, stack, flags, memory and ABI effects. It reduces
assembly-source duplication, not executable byte count. Subroutine outlining
requires a target-specific live-register/ABI analysis and is not performed by
this module.

Only indented instruction lines beginning with an ASCII letter are eligible.
Labels, directives, comments and macro escapes cannot enter a body. Existing
`.macro` definitions, macro-mode changes, conditional/repetition/include directives,
block comments, backslash escapes, semicolon separators and inline label statements
are rejected. Supported directives are `.text`, `.globl`, `.global`, `.type`,
`.size`, `.section`, `.align`, `.balign`, `.p2align`, `.file`, and `.ident`.
Embedded NULs and nonempty text lacking a final newline are also rejected.
Empty input is valid. The input is borrowed synchronously and remains
unchanged. The API is designed for MuLIB's GAS output, not arbitrary assembly
languages with different macro substitution rules.

Defaults search spans of 2..8 lines with at least two uses, a conservative positive
source-size saving, 1 MiB text and 16777216 work units. Input scans, span tests,
compared bytes and matched-span marking consume the work budget. Limits are literal.
Work exhaustion fails transactionally rather than returning truncated output.
The algorithm greedily takes the first longest usable span, with deterministic
source-order ties. Storage is O(lines + output bytes); search is bounded by the
work limit. Text expansion and allocation sizes are checked.

Zero-initialize `mulib_superinstr_result_t`; output owns a NUL-terminated string,
its byte length excluding NUL, and macro/use counts. On failure output is
unchanged. `mulib_superinstr_free` frees and resets it.

OptML invokes this pass after ABI-aware assembly emission when
`options.superinstructions` is enabled. The native backend regression executes
macro-expanded arithmetic and cross-assembles RV64 macros.

```sh
cc -std=c11 -Iinclude examples/modules/superinstr.c -lm -o superinstr
./superinstr
```
