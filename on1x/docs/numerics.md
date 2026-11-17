# M1 numeric values, decimal literals and MIR

The initial binary64 value slice is extended with decimal source literals,
dedicated float MIR operations, and trivially copied tuple MIR. Both scanners,
the canonical HIR, macro categories and native/Python PPX validators agree on the
new literal. HIR-to-C11 remains the reference path. This is not completion of M1:
f32, other integer widths, float arithmetic folding, records/ADTs, arrays/slices,
general ownership and generated safe foreign adapters remain unsupported.

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
preserves negative zero. Integer literals remain i64, including `0x1e3`.
No implicit integer/float coercion is added. `.5`, `1.`, hexadecimal floats,
suffixes and digit separators are not supported.

Each literal is limited to 1,024 bytes. Locale-independent C++ `from_chars`
converts decimal spelling to binary64 once per compiler consumer. The tested
compiler/toolchain profile rounds nearest/ties-even, including halfway cases
and subnormals. Emission stores the exact IEEE bits, not a decimal C literal;
the C compiler does not independently re-round source spelling. Invalid spelling,
overflow and underflow-to-zero are located compile-time errors. Exact zero with
a large exponent is valid; representable nonzero subnormals are valid.

The PPX v1 schema gains `FloatLiteral(spelling: FloatSpelling, location)` under
`NumericLiteral`, **not** `IntegerLiteral`. Its spelling is unsigned, matching
the token grammar; PPXs must represent negation as `Unary`. Existing integer
folders do not fold floats. Float literal macro patterns compare spelling;
they do not normalize equivalent decimal representations. The scanners preserve
existing dotted/ellipsis tokenization when no complete float token is present.

## Types and evaluation

`f64` is distinct from i64 and bool. It is trivially copied, has no drop action,
and works in ordinary functions, tuples, immutable captures, schemes and staged
code. No cross-thread semantics are added. Mixed integer/float arithmetic and
implicit conversions reject. `+ - * /`, unary `-`, and the six comparisons admit
same-typed f64 operands. `%` remains i64-only. Calls and operands retain the
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

`extern fn` f64 leaves/results map **exactly** to C `double`; flattened tuple
parameter patterns can contain f64 leaves. This is still a user-declared raw
escape hatch, not discovery or safe admission. No C `float`, long double,
pointer, foreign record, callback or aggregate layout is inferred. Separate-TU
tests compile actual C/C++ definitions with compatible prototypes. ExoML
`emit-raw` remains signed-integer/void only, and `emit-safe` remains closed.

MIR v2 admits dedicated f64 operations and recursively trivial tuples with
i64/bool/f64 leaves. Constants carry exact bits; integer opcodes never interpret
those bits as integer arithmetic. Its independent C++ evaluator and C11 emitter
are compared with HIR C11, including optimized paths, NaNs, signed zero, failed
casts and tuple copies. See [MIR profiles and serialization](backend-mir.md).
Float arithmetic/conversion/comparison effects are retained rather than folded.

Optional Nerotypical/Chisel adapters still reject f64; Novostone's v1 relational
mapping rejects MIR v2. General foreign calls remain unsupported in MIR. See
`../VERIFICATION.md` for measured tests and `capability-matrix.md` for remaining
milestones. No safe-import or concurrency gate is promoted by numeric tests.
