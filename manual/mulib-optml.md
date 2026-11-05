# mulib-optml

Annotation-driven LIR optimization and complete native-MIR-to-assembly pipeline.
The static catalogue contains 75 guarded shallow-tree rewrites, followed by
conservative dead-write elimination. All implementation code is in `include/`;
[`library/optim`](../library/optim/README.md) contains the loadable rule policy.

## Integration

Define these switches exactly once before including `mulib-optml.h`:

```
MULIB_IMPL_MIR MULIB_IMPL_MUBURG MULIB_IMPL_MACHINE MULIB_IMPL_LIR
MULIB_IMPL_ISCHED MULIB_IMPL_PARSEC MULIB_IMPL_ARCHML MULIB_IMPL_LOWOPT
MULIB_IMPL_ISEL MULIB_IMPL_PEEPHOLE MULIB_IMPL_STRBUF MULIB_IMPL_SUPERINSTR
MULIB_IMPL_OPTML
```

Compile the implementation as C11 and link `-lm`. See the complete runnable
[`examples/modules/optml.c`](../examples/modules/optml.c).

## Pipeline

```c
mulib_optml_options_t options = mulib_optml_defaults();
mulib_optml_result_t result = {0};
/* Optional: options.selection.arch = an already loaded ArchML document. */
mulib_mir_status_t status = mulib_optml_compile(
    &native_program, "my_function", &options, &result, &diagnostic);
/* On success: result.lir, result.assembly, result.length, result.stats. */
mulib_optml_free(&result);
```

Compilation performs:

1. Validate/clone native MIR and memoize costed target covers.
2. Annotate proven operand constants and ArchML scheduling facts.
3. Match peephole trees, retaining user constraints and stable source IDs.
4. Remove pure dead writes with conservative global live-out information and
   block-local overwrite analysis. Memory and calls remain observable.
5. Reselect and reschedule after **every** pass, invalidating stale facts.
6. Emit target assembly and optionally factor repeated sequences into macros.

Facts are refreshed before optimization as well, so an earlier custom pass can
mark its LIR unselected and hand it back. Reaching-definition matching never
crosses basic blocks or substitutes an overwritten input. Unknown initial values
are conservatively treated as unknown even though native MIR initializes slots
to zero. The optimizer retains a definition shared by other uses; dead-write
analysis may remove it only when legal. All removed records become NOPs.

`mulib_optml_run` accepts immutable LIR and returns an independently owned optimized
LIR, without emitting assembly. `mulib_optml_emit` sends selected/scheduled LIR
through the target emitter. Both built-in backends use stable spill slots and
handle stack alignment, register/stack arguments, symbols and block labels.
Supported built-ins are ELF AMD64 SysV and RV64IM LP64; additional machines need
an actual `mulib_machine_t` emitter, not only an ArchML document.

## Policies and bounds

`options.rules/rule_count` selects an ordered catalogue. The default points at
the 75 static peephole rules. Read `library/optim/default.optml` with the caller's
file tools and pass its bytes to `mulib_optml_parse` to load a policy. It accepts
one known name per line and `#` comments. Unknown/duplicate names fail with an
offset. Output is an owned array of rules with static borrowed names; release it
with `mulib_optml_rules_free` after optimization. Parsing is length-delimited,
with explicit byte/rule limits and transactional output.

Defaults use eight optimization passes, dead-write elimination, macro factoring
and a 1 MiB assembly limit. `passes=0` only refreshes selection/scheduling;
`dead_writes=0` disables that pass. Hitting the pass bound returns a valid,
possibly non-fixed-point result. Each pass bounds rule validation/matching,
register resets and dead-write scans by the selection work limit. Custom
catalogues are structurally validated even with zero passes. See [ISEL](mulib-isel.md) for structural/scheduling limits and
[superinstructions](mulib-superinstr.md) for macro limits. There are no hidden
file accesses, host feature probes or global optimization caches.

## Ownership, diagnostics and output

Zero-initialize all owners. `compile`/`run` preserve output and statistics on
failure. `mulib_optml_free` releases LIR arrays and assembly and resets the result.
The machine is borrowed; ArchML and input MIR are needed only during the call.
Use `mulib_mir_diagnostic_t` and `mulib_mir_status_string` for status and record
location; ignore the location for descriptor/global-limit errors.

`mulib_optml_write` writes the final owned assembly to a callback. A callback can
write a file, append a buffer, or copy into a caller-owned writable file mapping
(see `mulib-mmfile.h`). It returns nonzero on success; external partial output is
possible if a writer fails. Mapping assembly text stores source bytes: executable
mapping requires assembling/linking first, using the existing machine-code
loader/JIT APIs for a supported object format.

```sh
cc -std=c11 -Iinclude examples/modules/optml.c -lm -o optml
./optml library/archml/amd64.aml > twice.s
cc -c twice.s -o twice.o
```

Tests include all 75 rewrite properties, non-SSA register reuse, CFG loops,
randomized semantic equivalence, ArchML resources/barriers, allocation rollback,
writer failures, assembly macro expansion, native arithmetic/memory/calls and
RV64 cross-assembly. No unimplemented optimization names are advertised as passes.
