# Selecting compiler optimizations

`on1xc --list-optimizations` prints JSON for the linked compiler: each entry has
a stable `name`, a `stage`, and an `enabled` boolean. Add profile or selection
flags to see the resolved policy. Authored rule names come from the compiled
`.o1m` tables; MIR pass names come from the pass descriptors.

| `--opt-profile` | Authored processor rules | [`partial-eval`](../processor/PARTIAL_EVALUATION.md) | [`memgraph-bounds`](../MEMGraph/README.md) (C11 only, if built) | `decision-ssa` lowering | MIR pass pipeline |
|---|---|---|---|---|---|
| `default` (also when omitted) | on | off | off | on | off, except for `--optimize-ir` and `--dump-passes` |
| `debug` | off | off | off | off | off |
| `release` | on | on | on | on | on |

`--profile` is an alias for `--opt-profile` in the compiler CLI. Profile names
describe compiler transformations: selecting `debug` does not add host C debug
flags, and selecting `release` does not choose or change the output backend.
Source typing, ownership checks, checked arithmetic and MIR verification apply
in every profile.

```sh
on1xc --list-optimizations --opt-profile release --disable-opt local-slots
on1xc --emit-c example.on1x --profile debug --enable-opt integer_add_zero_right
on1xc --dump-processor example.on1x --disable-opt lazy_or
on1xc --dump-passes example.on1x --disable-opt all --enable-opt constant-fold
on1xc --emit-ir --ir-input example.mir --profile release
```

Precedence is **profile, then `all`, then named overrides**, regardless of
argument order. `--all-optimizations` and `--no-optimizations` are aliases for
`--enable-opt all` and `--disable-opt all`. Repeat the selector option for
multiple names. More than one profile or opposite settings for the same
selector are usage errors; an unknown name is an error even in an unused stage.

A broad profile or `all` selection applies to the stages the command actually
uses. An individual selector must refer to an applicable stage:

* Direct source C11, checking and `--dump-processor` accept authored rules
  and `partial-eval`.
* Direct source C11 emission and `--dump-memgraph` accept the `c11` stage
  selector `memgraph-bounds` when LLSynth-built MEMGraph is available.
* Source-to-MIR, including SDK MIR hooks and native emission, also accepts
  `decision-ssa` and the MIR passes `constant-fold`, `simplify-cfg`,
  `local-slots` and `dead-pure`.
* Serialized `--ir-input` accepts MIR passes only: it has no source tree to
  process or lower. Syntax and dependency inspection execute no optional passes.

`--dump-processor` reports active rules and their actual hits.
`--dump-memgraph` enables C11 bounds inspection unless explicitly disabled,
and reports the decisions for that emission.
`--dump-passes` reports the actual selected pipeline in its standard execution
order. When all MIR passes are disabled it verifies the input and reports
identical source and output MIR, zero work, and no events. `--optimize-ir`
retains its legacy MIR-mode restrictions and sets the **default** profile's
MIR baseline; it does not override an explicit `debug` profile or a disable
selector.

### Use in a project

In the root `On1xfile`, select a compiler preset and override individual
optimizations globally or for a named build profile:

```text
backend mir-c11
optimization-profile default
optimization "debug" "all" off
optimization "debug" "integer_add_zero_right" on
optimization "release" "all" on
optimization "release" "decision-ssa" off
```

`on1xc exec build --profile debug` now uses just the named generated rule.
`on1xc exec release` enables the other applicable passes but leaves decision
SSA off. `--opt-profile` and CLI pass selectors can override the manifest.
Inspect the resolved stages, pass names and build flags with
`python3 on1x/tools/on1x-project.py plan . --profile release --on1xc /path/to/on1xc`.
See the [On1xfile build profile reference](../stdlib/on1xfile/README.md#compiler-optimizations)
for layering, locking, and the distinction between project C flags and the
compiler's optimization profile.
