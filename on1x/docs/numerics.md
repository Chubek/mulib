# M1 numeric values, decimal literals and MIR

## Unsigned/binary32 extension contract v1

`u8`, `u16`, `u32`, `u64` are distinct, trivially copied unsigned value types.
`f32` is IEEE binary32, not an alias for f64. Unsuffixed literals retain their
existing types. Suffixed literals (`42u32`, `18446744073709551615u64`, `1.5f32`)
and explicit `u32_from_i64(42)`, `u64_from_bits(-1)` or `f32_from_f64(1.5)`
constructors are supported. There is no implicit numeric coercion, pointer-sized
integer or cross-width operator defaulting.

Same-width unsigned `+ - * / %` are checked (overflow, underflow and zero
divisors terminate with arithmetic exit 70). Unary unsigned negation rejects.
`uN_wrapping_add/sub/mul` explicitly operate modulo 2^N. `uN_and/or/xor/not`,
`uN_shl/shr` operate on bits: shift counts are i64 in `[0,N)`; checked left
shift rejects lost high bits. Comparisons and decimal printing preserve the
whole unsigned range. `uN_from_i64`, `uN_to_i64` and `uN_from_uM` are range
checked; `uN_from_bits(i64)` truncates modulo 2^N deliberately and
`uN_to_bits` exports the signed numeric bit encoding (u64 uses two's complement).

f32 arithmetic/comparisons follow the existing f64 policy at binary32 precision:
RNE, gradual underflow, no implicit contraction, canonical positive quiet NaN
`0x7fc00000`, signed zero, IEEE Inf/NaN behavior. Numeric conversions are
explicit: f32/f64 and integer-to-f32 round nearest/ties-even; float-to-integer
truncates toward zero and checks finite/range *before* C conversion. Binary32
bit import requires an i64 in `[0,2^32)`; bit export is a nonnegative i64.
Printing is exact fixed-width hex (six fraction digits, exponent -126 for
subnormals). The initial floating target remains Linux x86_64 GCC/Clang/SSE2
with a checked MXCSR environment; unsupported targets/modes reject.

The numeric intrinsics include `f32_from_i64`, `f32_to_i64`,
`f32_from_bits`, `f32_to_bits`, `f32_is_nan`, `f32_is_finite`, `f32_signbit`,
`f32_from_f64` and `f64_from_f32`. For every unsigned width, `f32_from_uN`,
`f64_from_uN`, `uN_from_f32` and `uN_from_f64` are explicit conversions.
Unsigned float inputs must lie in `[0,2^N)` before truncation (negative fractions
reject; negative zero is accepted). Narrowing f64 to f32 rounds, including to
zero/subnormals/Inf; it handles the FLT_MAX/Inf midpoint *before* any C cast.
Integer-to-f32 conversion is direct, not via f64, avoiding double rounding.

These types compose with functions, inference, tuples, by-value records/arrays,
immutable closures, modules and staging through HIR-to-C11. `ON1X_UNSIGNED_ABI_VERSION`
and `ON1X_F32_ABI_VERSION` are additive runtime ABI version 1. PPX v1 gains
`U8Type/U16Type/U32Type/U64Type/F32Type` constructors; rebuild schema-pinned
processors, which must reject unknown constructors rather than reinterpret them.
Both scanners now recognize the same suffixed numeric tokens. Scalar MIR v3
and Nerotypical independently check these numeric types; the evaluator performs
unsigned operations on full unsigned payloads and binary32 operations at actual
single precision. Chisel models numeric/bool tuples as trivially copied values
and checks versioned foreign evidence. Resource-bearing aggregates are not
admitted by this tuple profile.

Migration: the new numeric intrinsic names follow the existing reserved-global
rule (`intrinsic name cannot be redefined`). Rename any former user-defined
global with one of those names; lexical local shadowing remains unchanged.
Existing unsuffixed literals, i64/f64 operations, evaluation order and terminal
error policy are preserved. No old v1/v2 foreign evidence authorizes new types.

## Existing binary64 profile

The initial binary64 value slice is extended with decimal source literals,
dedicated float MIR operations, and trivially copied tuple MIR. Both scanners,
the canonical HIR, macro categories and native/Python PPX validators agree on the
new literal. HIR-to-C11 remains the reference path. This is not completion of M1:
narrow signed/pointer-sized integers, general language MIR, float arithmetic
folding and general ownership remain unsupported. The
bounded value-data profile and reviewed scalar FFI profiles are documented
separately, not inferred from passing numeric tests.

```on1x
fn half(value: f64) -> f64 { return value / 2.0; }
fn main() {
  let answer = half(85.0);
  println(answer);
  assert(f64_to_i64(answer) == 42);
}
```

## Literal syntax and rounding

`digits '.' digits [exponent]` and `digits exponent` denote `f64`, where
`exponent = ('e'|'E') ['+'|'-'] digits`. Examples: `0.0`, `42.5`, `1e3`,
`1.25E-2`. A sign is a unary operator, not part of the literal token; `-0.0`
preserves negative zero. Unsuffixed integer literals remain i64, including `0x1e3`.
No implicit integer/float coercion is added. Decimal/hex integer literals may
use `u8/u16/u32/u64`; decimal numbers may use `f32/f64`, including `1f32`.
Each suffix fixes the type; overflow is diagnosed for that exact width.
Negative unsigned literals reject as unsigned negation. Decimal f32 literals
round directly to binary32, not through binary64 (which could double-round).
`.5`, `1.`, hexadecimal floats, unknown suffixes and digit separators reject.
Hex digit sequences such as `0x1f32` and `0xf64` remain ordinary i64 literals.

Floating and suffixed unsigned literals are limited to 1,024 bytes.
Locale-independent C++ `from_chars`
converts decimal spelling to binary64 once per compiler consumer. The tested
compiler/toolchain profile rounds nearest/ties-even, including halfway cases
and subnormals. Emission stores the exact IEEE bits, not a decimal C literal;
the C compiler does not independently re-round source spelling. Invalid spelling,
overflow and underflow-to-zero are located compile-time errors. Exact zero with
a large exponent is valid; representable nonzero subnormals are valid.
The same policy applies directly to f32 using `from_chars<float>`; the 1,024-byte
limit includes any suffix. Unsigned literals are parsed with a full uint64
accumulator and checked against their declared width before lowering.

The PPX v1 schema gains `FloatLiteral(spelling: FloatSpelling, location)` under
`NumericLiteral`, **not** `IntegerLiteral`. Its spelling is unsigned, matching
the token grammar; PPXs must represent negation as `Unary`. Existing integer
folders do not fold floats. Float literal macro patterns compare spelling;
they do not normalize equivalent decimal representations. The scanners preserve
existing dotted/ellipsis tokenization when no complete float token is present.
The additive `UnsignedLiteral(UnsignedSpelling, location)` constructor is also a
`NumericLiteral`, deliberately not the legacy i64-only `IntegerLiteral` category.
Existing integer folders must leave it untouched. Canonical, standalone Python
and native On1x PPX validators preserve the suffix and source location.
Typed macro captures use the same categories. Unsigned literal macro patterns
compare numeric value **and suffix type**: `255u8` matches `0xffu8`, but not
`255`, `255u16` or a floating spelling. Legacy signed literal matching is unchanged.

## Types and evaluation

`f64` is distinct from i64 and bool. It is trivially copied, has no drop action,
and works in ordinary functions, tuples, immutable captures, schemes and staged
code. No cross-thread semantics are added. Mixed integer/float arithmetic and
implicit conversions reject. `+ - * /`, unary `-`, and the six comparisons admit
same-typed f64 operands. `%` admits only integer types, never f32/f64. Calls and operands retain the
existing left-to-right order. Numeric operator variables are monomorphic (not
numeric type classes); unconstrained numeric operands retain the historical i64
choice. Equality/printing alone still do not select a default type.

Each arithmetic operation rounds once to binary64, round-to-nearest/ties-to-even;
there is no contraction, reassociation or compiler-host float folding. Underflow
is gradual; overflow and nonzero/zero division yield infinity; invalid operations
yield NaN. NaN comparisons follow IEEE unordered rules (`!=` is true; all other
comparisons are false). Positive and negative zero compare equal. NaN payloads
and signs are not semantic values: bit import/export and arithmetic canonicalize
them to positive quiet NaN `0x7ff8000000000000`.

The initial supported target is **Linux x86_64, GCC/Clang, SSE2 binary64**.
Generated float code checks a versioned runtime ABI and binary64 layout/evaluation
properties. Other targets reject at C compilation, not silently approximate.
The runtime checks MXCSR before arithmetic, comparisons and numeric conversions:
non-nearest rounding, flush-to-zero/denormals-are-zero and unmasked exceptions
terminate with arithmetic error 70. Raw foreign code must preserve this environment
or restore it before further float operations. Fast/finite-only math compilation
is rejected (including GCC's exposed unsafe-math/signed-zero relaxations).
Clang's primitive implementations explicitly restore precise floating semantics
because it exposes no macros for its individual no-honor-NaN/signed-zero flags.
Existing non-float generated code does not gain a float ABI guard.
The MIR evaluator checks the same host evaluation profile; an x87/excess-precision
compiler build rejects floating operations with a located unsupported diagnostic.

## Intrinsics

| Intrinsic | Contract |
|---|---|
| `f64_from_i64(i64) -> f64` | Explicit nearest/ties-even conversion; may lose precision |
| `f64_to_i64(f64) -> i64` | Truncate toward zero; reject NaN, infinity and values outside `[-2^63, 2^63)` **before** casting |
| `f64_from_bits(i64) -> f64` | Interpret the integer modulo 2^64 as IEEE bits; canonicalize NaNs |
| `f64_to_bits(f64) -> i64` | Export IEEE bits as a signed two's-complement numeric encoding; canonicalize NaNs |
| `f64_is_nan(f64) -> bool` | NaN classification |
| `f64_is_finite(f64) -> bool` | Neither NaN nor infinity |
| `f64_signbit(f64) -> bool` | Sign, including negative zero (NaNs canonicalize positive) |
| `println(f64)` | Exact, locale-independent hex: sign, `0x0`/`0x1`, 13 fractional hex digits, `p` signed decimal exponent; `inf`, `-inf`, `nan` |

Zero prints with exponent `+0`; subnormals use `-1022`. These fixed-width hex
strings are diagnostic output, not a newly accepted source-literal syntax.
Bit transport provides exact serialization for finite values and signed zero.
Conversions that fail are terminal, like existing checked i64 arithmetic; they
do not provide recoverable Result semantics or unwind language owners.

## Raw ABI and unsupported boundaries

`extern fn` f64/f32 leaves/results map **exactly** to C `double`/`float`;
unsigned leaves map to exact C fixed-width unsigned types. Flattened tuple
parameter patterns can contain these numeric leaves. This is still a user-declared raw
escape hatch, not discovery or safe admission. No long double,
pointer, foreign record, callback or aggregate layout is inferred. Separate-TU
tests compile actual C/C++ definitions with compatible prototypes. ExoML
`on1x-bind emit-raw` generates exact signed/unsigned-integer, `_Bool`, binary32
`float` and binary64 `double`
adapters with type/size/alignment/format checks. `emit-safe` additionally requires
all four [reviewed scalar admission gates](contracts.md#reviewed-scalar-only-safe-admission);
v2 evidence carries exact `bool`/`f64` types; v3 adds `u8/u16/u32/u64/f32`.
This does not admit floating buffers,
aggregates, resources, callbacks or other floating formats.

MIR v2 admits dedicated f64 operations and recursively trivial tuples with
i64/bool/f64 leaves. Constants carry exact bits; integer opcodes never interpret
those bits as integer arithmetic. Its independent C++ evaluator and C11 emitter
are compared with HIR C11, including optimized paths, NaNs, signed zero, failed
casts and tuple copies. See [MIR profiles and serialization](backend-mir.md).
Float arithmetic/conversion/comparison effects are retained rather than folded.

MIR v3 adds u8/u16/u32/u64/f32 scalars and nested numeric tuples. Its `numeric`
opcode carries a bounded, fixed-allowlist descriptor with an exact structural
signature; this is not an arbitrary runtime or foreign function name. All new
arithmetic, wrapping/bit operations and conversion intrinsics have independent
reference execution and C11 emission. Conservative effect tokens preserve
numeric failure and floating-environment order. v1/v2 serialization is unchanged;
v3 constants carry exact typed unsigned/IEEE payloads, never signed arithmetic
substitutes. Resource/closure/data-model MIR and foreign calls remain unsupported.

The optional Chisel adapter admits numeric scalars and numeric/bool tuples,
including numeric/bit/classification intrinsics. Nerotypical builds distinct
numeric constructor/equality obligations and checks literal/operator/call types;
this does not prove IEEE arithmetic, ownership or foreign implementation safety.
Novostone's v1 relational mapping rejects MIR v2/v3. See
`../VERIFICATION.md` for measured tests and `capability-matrix.md` for remaining
milestones. No safe-import or concurrency gate is promoted by numeric tests.
