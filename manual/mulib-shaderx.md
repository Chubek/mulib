# mulib-shaderx

A typed expression-graph API and high-level shader language, compiling to
**GLSL 330 core** or **HLSL 5 source**. Define `MULIB_IMPL_SHADERX`,
`MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`. Parsec's base combinators perform
tokenization; bounded precedence parsing constructs the same checked graph
used by the C API. No third-party compiler is required to generate source.

## Language

```text
fragment {
  input vec2 uv;
  uniform float time;
  let wave = sin(time) * 0.5 + 0.5;
  return vec4(uv, wave, 1);
}
```

The complete supported grammar is:

```text
program     := ('vertex' | 'fragment') '{' declaration* 'return' expression ';' '}'
declaration := ('input' | 'uniform') type identifier ';'
             | 'let' identifier '=' expression ';'
type        := 'float' | 'vec2' | 'vec3' | 'vec4'
expression  := decimal | identifier | '(' expression ')' | ('+'|'-') expression
             | expression ('+'|'-'|'*'|'/') expression
             | function '(' (expression (',' expression)*)? ')'
function    := 'vec2'|'vec3'|'vec4'|'dot'|'normalize'|'sin'|'cos'|'mix'|'min'|'max'
```

Unary operators bind most tightly, then multiply/divide, then add/subtract;
binary operators associate left. Decimal literals accept fractional parts and
signed decimal exponents (`.5`, `1.`, `2e-3`). Numeric conversion is locale-independent;
finite float-range values are required and tiny values may underflow to zero.
Whitespace and `//` line comments are accepted. Identifiers use ASCII letters,
digits and underscore, starting with a letter/underscore. Duplicate/unknown
names, mismatched types, extra input and unsupported tokens produce diagnostics.

Arithmetic broadcasts scalars to vectors; different vector widths cannot mix.
Vector constructors flatten argument components to exactly the destination width,
or replicate one scalar. `dot` takes two equal-width vectors and returns float.
`normalize` takes a vector. `sin/cos` are component-wise; `min/max` allow scalar
broadcast. `mix(a,b,t)` requires equal a/b types, and scalar or matching t.
Division by zero/zero normalization are left to the target's runtime semantics.
Each stage returns exactly one vec4: clip-space position or fragment color.

## API, ownership and limits

`create` returns an owned graph; `destroy` frees it. Constant/input/vector/apply
return a one-based expression ID or zero without mutation. IDs are local to a
graph; do not mix them across graphs. Inputs/uniforms have unique copied names.
`output` accepts a vec4 ID. C and language paths enforce the same type rules.

Maximum graph size: 1024 nodes, 64 input/uniform bindings, 63 identifier bytes.
Parsing additionally limits source to 64 KiB, token captures to 16384, expression
nesting to 64 and symbols to 256. Graphs copy all retained data and outlive source.
Parse errors contain a static message, status and zero-based byte offset. NULL
source is only valid with length zero (which is a syntax error). Graph builders
do not allocate after graph creation. Parser and output builders can fail on OOM.

`emit` appends transactionally to an initialized `mulib_strbuf_t`, returning 1
success or 0 invalid graph/target/allocation failure. Caller frees the string
builder. Generated identifiers are `sx_in<ID>`/`sx_v<ID>`, making source names
independent of backend reserved words. `find_input(program,name)` resolves an
input/uniform's expression ID so applications can bind `sx_in<ID>` after parsing.
Constants use bit-preserving uint-to-float
intrinsics; binary32 float is required. GLSL vertex inputs get sequential locations;
fragment inputs use ordinary varyings. HLSL inputs get sequential TEXCOORD semantics,
uniforms occupy `SXUniforms : register(b0)`, and output is SV_Position/SV_Target0.
Programs are single-stage; application pipeline linkage and resource binding are
caller responsibilities. No loops, branches, matrices, textures, swizzles, SPIR-V
or GPU execution are implied by this expression profile.

`examples/modules/shaderx.c` prints GLSL. `tests/test_graphics_languages.c` covers
API/text construction, diagnostics, bounds and both backends. Its optional
`fragment|vertex output-file` mode emits shaders for external compiler validation.
