# Compiler and VM building blocks

These modules cover source handling, expression parsing, compiler analyses,
register placement, checked stack bytecode, and explicit VM storage.
They are independent building blocks rather than an end-to-end language compiler.

## Integration and ownership

Use ISO C11. Define each `MULIB_IMPL_<MODULE>` in exactly one translation unit;
dependency implementations are explicit, never enabled by another module.
The standalone examples show complete switch configurations. C++ clients may
include declarations and link to C implementations. Implementation switches
may also be enabled on a second inclusion after the declarations.

Initialize owned containers with `{0}` before first use. Their matching `free`
functions release storage, reset the container, and accept NULL. Do not shallow-copy
owning containers, alter their allocation bookkeeping, or retain pointers into
resizable arrays across mutations. Caller-provided buffers must actually contain
the stated number of elements. Output pointers must not alias container metadata
or input arrays unless the module explicitly supports in-place operation.

Unless otherwise specified, integer predicates return 1 on success and 0 on
invalid input, allocation failure, or a violated bound. Output scalars and owned
inserts are committed on success. Text printers may leave a terminated prefix;
pipelines and traversal callbacks retain effects already performed. Containers
are not internally synchronized. Callbacks must not mutate the pipeline or AST
being traversed.

## Workflows

- Frontend: [source-manager](mulib-source-manager.md), [lexer](mulib-lexer.md),
  [parser-pratt](mulib-parser-pratt.md), [symboltable](mulib-symboltable.md),
  [name-resolution](mulib-name-resolution.md), [typecheck](mulib-typecheck.md),
  [consteval](mulib-consteval.md).
- Analysis: [cfg](mulib-cfg.md), [dominance](mulib-dominance.md),
  [postdominance](mulib-postdominance.md), [loops](mulib-loops.md),
  [liveness](mulib-liveness.md), [dce](mulib-dce.md), [range-analysis](mulib-range-analysis.md).
- Backend: [regalloc](mulib-regalloc.md), [frame-layout](mulib-frame-layout.md),
  [callconv](mulib-callconv.md), [branch-layout](mulib-branch-layout.md).
- Execution: [bytecode-builder](mulib-bytecode-builder.md),
  [bytecode-verify](mulib-bytecode-verify.md), [bytecode-eval](mulib-bytecode-eval.md),
  [vm-module](mulib-vm-module.md), [gc](mulib-gc.md).

## Validation

`tests/test-compiler-*.c` contains eight functional and regression suites, discovered
by Make and CMake. `python3 tools/check_compiler.py` additionally compiles every
module independently, links C implementations to C and C++ clients, and runs all
60 module examples. Add `--sanitizers` to check memory and undefined behavior.

[Full module index](index.md) · [Integration guide](integration.md)
