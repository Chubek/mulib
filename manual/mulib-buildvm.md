# MuLIB BuildVM

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-buildvm.h` · **Switches:** `MULIB_IMPL_BUILDVM`,
`MULIB_IMPL_BUILDGRAPH`, `MULIB_IMPL_ARTIFACT`, `MULIB_IMPL_VCDIFF`.

BuildVM is a deterministic, finite artifact-register machine for hermetic build
transformations. Programs can only access declared BYTES inputs and declared
output slots. There are **no** host file, environment, process, clock, randomness,
dynamic callback, or network instructions. It is not a general-purpose compiler
VM or a sandbox for arbitrary C, shell scripts or native tools. It has no loops
or hidden filesystem/materialization/cache policy.

## Program and instructions

`mulib_buildvm_program_t` borrows an immutable instruction array and specifies
1..65536 registers, 0..65536 instructions and a byte budget. Registers start empty.
All operands are zero based; input/output slots follow graph edge insertion order.
Use zero-initialized instruction structs and fill only needed fields:

| Opcode | Operation |
|---|---|
| INPUT | `register[dst] = declared_input[a]` (retained, BYTES only) |
| LITERAL | copy `data[0:length]` into register dst |
| CONCAT | concatenate registers a and b into dst |
| SLICE | copy `register[a][offset:offset+length]` into dst |
| DIFF | encode VCDIFF from source register a to target register b into dst |
| PATCH | decode delta register b against source register a into dst; length is output capacity |
| EMIT | stage register a in declared output slot b |

Destination/source aliasing is valid: results are constructed before replacement.
New byte values use instruction `id/type` or defaults `buildvm:value`/`file`;
DIFF defaults to `diff/vcdiff`. INPUT retains original metadata. Empty literal,
slice, concatenation and delta targets work. DIFF uses the existing prefix-copy
encoder; PATCH uses the documented VCDIFF decoder profile, rejecting unsupported
encodings. Archives are ordinary BYTES values, with metadata intact when forwarded;
byte transformations do not implicitly rewrite an archive.

`validate` checks structural operands before graph registration; execute also
validates all binding indices before processing instructions. Reading an empty
register, non-BYTES input or out-of-bounds slice fails. Validation does not simulate
register initialization.

`max_bytes` defaults to 64 MiB and conservatively bounds cumulative loaded/created
bytes, **not just current register occupancy**. Repeated loads charge again;
DIFF charges `target_length + 128`, PATCH charges its supplied capacity. EMIT
shares references without another byte charge. This bounds outputs even when a
register is repeatedly overwritten after emitting. Temporary construction can
require one extra copy of an operation's result; the cap is not an exact allocator
or process memory limit. Program/register/graph metadata are separate bounded
allocations. Unknown instructions/capabilities are never silently ignored.

## Graph integration and errors

`mulib_buildvm_task(graph, name, &program, &id)` validates and registers a borrowed
program. Keep code, literal buffers and metadata strings immutable/alive through
all graph runs. A program may be shared across concurrent task instances.
Use `mulib_buildgraph_build` with NULL scheduling for serial execution, or the
PolyTask adapter for parallel branches. BuildVM has no threading/native execution
dependency. Existing serial `execute` can also run registered tasks, with a
non-NULL legacy executor argument as required by its API.

The callback `mulib_buildvm_execute` returns OK, INVALID, MEMORY or LIMIT. The graph
records this code, discards every staged output on failure, blocks descendants,
and continues independent branches. Every declared output must be emitted before
success; omissions are graph MISSING errors. Earlier EMIT instructions never
publish partial graph products. Direct callback users must supply graph-managed
step storage and honor this rollback contract.

```sh
cc -std=c11 -Iinclude examples/modules/buildvm.c -o /tmp/opencode/buildvm
/tmp/opencode/buildvm
```

`tests/test_build_tasks.c` covers serial/parallel builds, target closure, all
operations including binary diff/patch, bad registers/slices, limits and rollback.
This is a focused build-data VM, not a claim of arbitrary native build hermeticity
or a production distributed build system.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-buildvm.h`](../include/mulib-buildvm.h) |
| Implementation switch | `MULIB_IMPL_BUILDVM` |
| Repository-local includes | [mulib-buildgraph](mulib-buildgraph.md), [mulib-vcdiff](mulib-vcdiff.md) |
| Complete example | [`examples/modules/buildvm.c`](../examples/modules/buildvm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/buildvm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
