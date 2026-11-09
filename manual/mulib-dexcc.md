# mulib-dexcc

[Manuals](README.md) · [Android runtime](mulib-art.md)

IR-neutral Android bytecode compilation tools. The implementation is original C11,
reviewed against `standards/android/android-dex.txt` and `android-bytecode.txt`.
It does not invoke D8, Java, an NDK, or any external compiler.

## Integration and profile

Define `MULIB_IMPL_DEXCC`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_AUTHCRYPTO`, and
`MULIB_IMPL_ADLER32` in one translation unit. No non-MuLIB implementation dependency
is required. See [the complete example](../examples/modules/dexcc.c).

The executable/compiler method profile is **static `(I...I)I`**: Java 32-bit int
registers, constants, moves, negation/complement, add/subtract/multiply/divide/remainder,
bitwise operations, shifts, six two-register comparisons, goto and return.
There are no objects, fields, invokes, wide/floating values, payloads, handlers or
debug records. Unknown bytecodes are diagnosed, never translated to NOP.

`mulib_dexcc_instruction(op)` supplies canonical unused fields. Value operations
write `a`, read `b,c`; branches read `a,b` and use `target`; return reads `a`.
Targets are **instruction ordinals**. Unused registers/literals are zero and unused
targets are `MULIB_DEXCC_NONE`. Parameter words occupy the last registers.

## Compiler hooks and encoding

Any IR can use `mulib_dexcc_ir_t`: implement its `read(user,index,out)` hook after
instruction selection and virtual-register assignment. `mulib_dexcc_import`
calls it exactly once per record, copies the result, and verifies the complete
method. There is no dependency on one particular IR representation.

Validation checks every instruction, reference, control-flow boundary, reachable
instruction and **all-path register initialization**, including loops and joins.
It rejects unreachable instruction records and fallthrough beyond the image.
Loops need not terminate. Division by zero remains a runtime fault, not a compiler
error. The instruction/register product must fit `limit` analysis cells.

`encode` appends raw, little-endian DEX `insns`, not a `code_item`. It uses move/16,
const, 23x arithmetic, unary 12x, goto/32, and return encodings. Conditionals always
relax to an inverted short conditional around a goto/32, so distant targets work.
Encoding constraints still apply: arithmetic/constant/return registers <=255,
unary/comparison registers <=15, and move registers <=65535. The IR adapter must
assign suitable low registers or insert moves; encoding never silently truncates.

`decode` also accepts move/from16, compact constants, integer /2addr and /lit8/16,
short gotos, and zero comparisons. Literal/zero forms expand through one private
scratch register, with incoming parameter registers remapped correctly. Branches
may not enter an operand or the middle of an expansion. A method already using
65535 registers cannot add this scratch register.

## Complete DEX files

`file` emits a DEX035 file with one public final class extending
`java.lang.Object`, containing one public static integer method. Static-only classes
need no constructor. Names/descriptors use the documented ASCII subset: letters,
digits, `_`, `$`, `-`, plus class-package `/` separators and `L...;` framing.
Defining `java.lang.Object` itself is rejected. Arbitrary MUTF-8 names are not
claimed. The writer generates sorted/deduplicated string/type IDs, a shorty and
parameter type list, method/class definitions, encoded class data, aligned code,
offset-sorted map entries, SHA-1 and Adler-32. Hashes detect corruption, not trust.

`check_file` is explicitly an **envelope check**, not a complete DEX verifier:
magic/version, header, section bounds, data/map bounds and hashes. It accepts
little-endian 035, 037, 038, 039 and 040 envelopes; 041 containers/reverse endian
are unsupported. [ART](mulib-art.md) separately validates the selected executable
method. This writer does not build APKs or sign application packages.

## Ownership, bounds and errors

Input descriptors/arrays and adapter data are borrowed only during the call.
Imported/decoded buffers own their instruction arrays; zero initialize, never copy
a live owner, and use `mulib_dexcc_free`. Buffer-producing operations are atomic
appends; existing output prefixes survive failure. Input may alias output because
emission is staged. `limit=0` is not unlimited: it bounds source/encoded units,
file bytes and generated arrays, with instruction*register analysis bounded too.
Allocation overflow and OOM are distinct LIMIT/NOMEM statuses. Descriptors must
refer to real readable C arrays, not arbitrary pointers. Runtime is not a sandbox.

Diagnostics identify instruction ordinals during verification/encoding or source
code-unit offsets while decoding. Header checks report byte offsets. Adapter
failure has its own status. Free is NULL-safe and resets the owner.

Evidence: `tests/test_android_runtime.c`, `tests/test_android_alloc.c` and
`tools/check_android.py` cover IR hooks, register initialization, branch relaxation,
parameter remapping, malformed inputs, allocation failure, file loading and
independently computed Python SHA-1/Adler-32/layout checks. The larger
[`library/archml/dex.am`](../library/archml/dex.am) encoding profile is metadata;
its 98 forms do not expand this runtime's supported subset.
`tests/test_android_hostile.c` additionally mutates rehashed envelope/metadata
records and bounded raw bytecode under ASan/UBSan.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dexcc.h`](../include/mulib-dexcc.h) |
| Implementation switch | `MULIB_IMPL_DEXCC` |
| Repository-local includes | [mulib-adler32](mulib-adler32.md), [mulib-authcrypto](mulib-authcrypto.md), [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/dexcc.c`](../examples/modules/dexcc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dexcc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
