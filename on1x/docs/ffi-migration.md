# MuLIB foreign interfaces: migration guide

For a portable kernel, import its standard module directly:

```on1x
import foreign stdffi.clamp as Clamp;

fn main() {
    println(Clamp.add_i64(40, 2));
}
```

`on1xc --run-ir program.on1x` evaluates the checked kernel and prints `42`.
`on1xc --emit-ir program.on1x -o program.mir` keeps the portable body and its
directives. Pass that file to `on1xc --ir-input --emit-ir-c program.mir` or
`on1xc --ir-input --run-vm program.mir` to select a product later. No target
catalogue, native review file or `unsafe` block is part of this import.
`Clamp.i64(value, low, high)` still checks `low <= high`; a failing call traps
after evaluating its arguments once in source order.

For a native direct-C11 executable configure On1x with `ON1X_WITH_CEPHYR=ON`
and the installed `Cephyr_DIR`. Then:

```sh
python3 on1x/tools/on1x-build.py program.on1x --on1xc /path/to/on1xc \
    --backend c11 --cc cc -o program
./program
```

On AMD64 Linux, the first native kernel call compiles the embedded MuLIB
implementation with Cephyr and loads its shared module. The program reuses
that module for later calls. With Cephyr disabled, the evaluator, VM, and
`--backend mir-c11` can still execute the portable body. See the
[runtime SDK](../runtime-sdk/README.md) for the native API.

The [standard FFI catalogue](../stdffi/README.md) describes generation and
the supported kernel bodies. An unlisted kernel or untranslatable export
does not become callable because Cephyr parsed its signature.

## Separate native KernelBind workflows

The migration keeps the source spellings `import kernel`, `unsafe import
kernel`, and their `[dynlib]` forms. `as` keeps the alias local to the importing
file. An unsafe import is not permission to call it anywhere: supported calls
are direct and require a lexical `unsafe` block. The existing clamp example
uses:

```on1x
unsafe import kernel mulib.clamp as Clamp;

fn main() {
    let answer = unsafe { Clamp.add_i64(40, 2) };
    println(answer);
}
```

Use the [reference build instructions](kernel-bind.md#existing-static-build)
and the [existing examples](../examples/kernel/README.md) for current
invocations. For `Clamp.i64(value, low, high)`, On1x checks `low <= high` after
evaluating the arguments and before calling C; `unsafe` does not skip that
check. Static and dynamic linkage are distinct build choices. An approval for
one does not authorize the other.

## What changes for maintainers

1. `MANIFEST.yaml` remains the source of module registration and native build
   requirements. A registration is not a statement that On1x can call a module.
2. Use the registration workflow in [CONTRIBUTING.md](../../CONTRIBUTING.md).
   The compiler does not load an external descriptor catalogue. The SDK's
   ABI-compatible v2 descriptor lookup returns `ON1X_SDK_UNSUPPORTED`.

## Reading discovery and review results

| Result | What it establishes |
| --- | --- |
| Registered | The module has a canonical manifest entry. |
| Projected | A declaration was resolved for one input and target. |
| Representable | A supported native-to-On1x type mapping exists; this does not authorize a call. |
| Callable with `unsafe` | The supported profile passes its native and ABI checks and requires lexical `unsafe`. |
| Callable with independent review | The exact supported subject also has a valid approval from the operator-selected authority. |

C types do not establish who owns a pointer, whether a callback is retained,
whether an output is initialized after failure, or what effects a call may
have. Unsupported and discovery-only declarations stay non-callable. A
generated file, snapshot, checksum or test key cannot authorize a safe import.
The [existing review workflow](kernel-review.md) explains independent
Ed25519 authorization; do not reuse its approval for a changed binding,
artifact, compiler configuration or linkage.
