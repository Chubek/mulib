# mulib-distribtask

[Manuals](README.md) · [Library index](index.md)

Compile an owned immutable task DAG from **both** a JSON/YAML configuration
document and trusted Mune source. This is not a C header parser: task files
contain Mune/Lua text, conventionally `.distribtask` or `.lua`, regardless of
extension. `mulib-distribtask.h` is the C API. File loads are bounded and
transactional; callers get a complete plan or an owned error diagnostic.

## Configuration and Mune

```yaml
cluster: example
nodes: [worker-a, worker-b]
attempts: 3
lease_ms: 30000
```

Configuration uses the internal JSON DOM or documented internal YAML subset
(not full YAML). `cluster` and 1..32 unique `nodes` are mandatory. Optional
attempts is an integer 1..100, default 3; lease_ms is an integer 1..86400000,
default 30000. Extra configuration fields remain available to the profile.
IDs contain 1..63 ASCII alnum/underscore/dot/hyphen bytes, excluding `.` and `..`.
No path or shell interpretation occurs. Secrets belong outside this document.

```lua
distrib_task('prepare', 'worker-a', 'mune', 'run', "return 'ready'")
distrib_task('start', 'worker-b', 'container', 'start', 'web')
distrib_depends('start', 'prepare')
```

The optional profile runs first, then task source in the same bounded Mune
state. `distrib_config` is a converted table of the required document, with
1-based arrays and Lua nil for JSON null. All task arguments are NUL-free
strings; kinds are `mune`, `daemon`, `container`, `ocicontainer`, `vmm`; action
is a portable ID <=31 bytes. Payload is validated NUL-free UTF-8 text <=64 KiB.
Node must be in configuration. Dependency declarations can precede definitions.
Duplicate task IDs/edges, absent references, self-edges, cycles and limits fail.
Actions are validated/executed by the worker, not during compilation.

`library/ldistrib/cluster.lua` supplies `cluster_task`, `mune_task`, `daemon_task`,
`container_task`, `oci_task`, `vmm_task`, and `replicate` helpers. Pass its path
as the profile argument to `load_file`; no working-directory-dependent implicit
module search is needed. The supplied cluster JSON/YAML and example task file
are equivalent runnable compilation fixtures. Empty task source is a valid
empty plan; missing/empty configuration is not.

Limits: config/profile/source each 1 MiB; 256 tasks; 1024 dependency declarations;
64 KiB payload per task. Mune's heap/step/depth limits also apply. Mune has host
capabilities: **profiles and tasks are trusted programs, not a sandbox**.
Compilation does not deliberately launch work, but arbitrary Mune host calls
can have side effects. Do not compile untrusted programs in a privileged host.

Plan owns copied strings/DOM; getters borrow immutable storage. Destroy the plan
only after all borrowing coordinators have been destroyed. Errors copy their
message before the Mune state is torn down. Return NULL for invalid input,
I/O, script, graph, limit or allocation failure; error.code/message distinguish
the reason. Output diagnostics do not borrow interpreter memory.

## Implementation placement

Define `MULIB_IMPL_DISTRIBTASK`, `MULIB_IMPL_JSON`, `MULIB_IMPL_YAML`,
`MULIB_IMPL_UTF8`, plus Mune's full closure: `MULIB_IMPL_MUNE`, `MULIB_IMPL_MIR`,
`MULIB_IMPL_VMWEAVE`, `MULIB_IMPL_EGRAPH`, `MULIB_IMPL_REWRITE`,
`MULIB_IMPL_DYNCALL`, `MULIB_IMPL_JIT`, `MULIB_IMPL_DYNLOAD`,
`MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_AST`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_GLOB` once. Linux links `-lm -pthread -ldl`.
`examples/modules/distribtask.c` is a standalone checked example;
`examples/distrib-common.h` includes additional coordinator dependencies.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-distribtask.h`](../include/mulib-distribtask.h) |
| Implementation switch | `MULIB_IMPL_DISTRIBTASK` |
| Repository-local includes | [mulib-mune](mulib-mune.md), [mulib-utf8](mulib-utf8.md), [mulib-yaml](mulib-yaml.md) |
| Complete example | [`examples/modules/distribtask.c`](../examples/modules/distribtask.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/distribtask.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
