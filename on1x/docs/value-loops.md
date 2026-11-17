# Value-loop increment (G2)

This scoped continuation of `ON1X_CORE_SYNTAX_ADDITIONS.md` extends the existing
compiler, not PPX or the runtime. The other requested owning-list, cleanup/error,
concurrency and nominal-ADT features still need their own dependency gates.

## Implemented semantics

`let answer = loop { break with 42; };` evaluates a loop expression. A label may
precede an expression loop (`let x = search: loop { ... };`). Value exits are
`break with expr;` and `break label with expr;`; their operands evaluate once,
before releasing the scopes being exited. `continue` does not produce a value.
Normally completing value exits must have one common type, without coercions.
A bare break makes the result void, while value operands still execute and are
typechecked. Unique JSON owners/views cannot be loop results or discarded value
exits. Ordinary shared/RC and bounded by-value results keep their current rules.

For compatibility, existing statement loops/while/for keep their representation
and bare-exit behavior. A value exit must target an expression loop; use
`(loop { break with 42; });` to discard a value. As in existing statement loops,
return analysis remains conservative for loops with no value exits. This does
not introduce a general bottom/`never` type.

Source constructs lower into existing HIR regions, single-initialization storage,
while/branch/leave edges and a fatal unreachable fallback. Initialization and
lexical control are independently verified. C11 uses existing cleanup edges;
scalar/numeric tuple MIR uses existing slots/CFG with no format/opcode change.

Validation covers both scanners, strict GCC/Clang C11, MIR evaluator and
optimized/serialized MIR, nested/outer exits and for latches, macro/PPX/staging/
generic/import cases, malformed HIR/wire, lexical RC/JSON cleanup, OOM injection
and generated-program ASan/UBSan. Final full suites passed 47/47 (GCC), 46/46
(fallback Clang), 56/56 (SDK) and 56/56 (SDK ASan/UBSan); focused Release passed
6/6. Exact evidence and unsupported profiles are in `../VERIFICATION.md`.

## Run

```sh
python3 on1x/tools/on1x-build.py on1x/examples/value_loops.on1x \
  --on1xc build/on1x/on1xc -o build/value-loops
build/value-loops
ctest --test-dir build/on1x -R 'on1x.value_loop' --output-on-failure
```

Output: `49`, `42`, `8`. Scalar/numeric tuple programs also accept the build
helper's `--via-mir --optimize-ir` path. Bounded aggregates and managed results
retain C11 support but explicitly reject unsupported MIR/independent adapters.
