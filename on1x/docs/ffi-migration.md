# MuLIB foreign interfaces: migration guide

**Migration in progress.** The [KernelBind guide](kernel-bind.md) and
[registration guide](kernel-registration.md) still describe the reference
workflows. The [retirement manifest](ffi-retirement.md) tracks what must move
before those workflows can be removed. This page does not announce new
callable profiles or retired commands.

## What stays the same for callers

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
2. The new generator uses Cephyr's resolved C11Interface projection from the
   canonical headers, with an explicit target and preprocessing environment.
   A projection can include declarations from dependencies and system headers;
   it is not an export list or evidence that the implementation can compile.
3. The intended generated location is `on1x/stdffi/mulib/`, with a coverage
   and provenance index beside the `.ffi` files. Change canonical inputs
   rather than editing generated files. Authored semantic overrides belong
   under `on1x/stdffi/overrides/mulib/`; they supply facts such as clamp's
   precondition, not C signatures or review authority.
4. LLSynth and FFIHandler turn supported declarations into compiler
   descriptors at build time. Checked On1x HIR and the C11/native boundary
   still enforce call, ABI, and ownership rules. The application does not run
   Cephyr's declaration projector or LLSynth at runtime.

The final regeneration and application-build commands will be documented after
qualification. Until then, keep using the manifest registration workflow in
[CONTRIBUTING.md](../../CONTRIBUTING.md) and do not
replace a working build with a command from the design directive.

## Reading discovery and review results

| Result | What it establishes |
| --- | --- |
| Registered | The module has a canonical manifest entry. |
| Projected | Cephyr resolved declarations for this input and target. |
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
