# MuLIB BuildPlan

[Manuals](README.md) · [Library index](index.md)

**Header:** `kernels/mulib-buildplan.h`  
**Switch:** `MULIB_IMPL_BUILDPLAN`

BuildPlan is an owned, bounded target dependency graph for build-system
frontends and planners. It is based on the graph resolution, resource scheduling,
critical-path and queueing primitives in `.sot/buildsys`. It does not execute
commands or materialize artifacts; use [BuildGraph](mulib-buildgraph.md),
[Task](mulib-task.md), or an application backend for execution.

## Graph and ownership

`mulib_buildplan_create(targets, dependencies)` allocates fixed capacities.
Targets own their names and optional tags, borrow optional application action
pointers, and receive stable numeric IDs. Repeating a target declaration returns
the existing ID, allowing parsers to merge declarations. Dependencies point from
prerequisite to dependent and duplicate edges are idempotent.

Names must be nonempty, contain no control bytes, and be at most 4095 bytes.
They are logical build names, not validated filesystem paths. Mutation during a
scheduling callback returns `MULIB_BUILDPLAN_BUSY`.

`mulib_buildplan_order` produces a deterministic dependency-first topological
order. Lexicographic names break ready ties. `mulib_buildplan_cycle` returns one
cycle with the first ID repeated at the end and supports a size-query call with a
NULL output. It is intended for diagnostics, not enumeration of every cycle.

## Resources and critical path

Each target defaults to one CPU, no memory request, priority zero and duration
zero. `mulib_buildplan_schedule` emits dependency-ready waves under a positive
CPU budget and an optional memory budget. Higher priority is considered first,
then lexicographic name. This is deterministic greedy packing, not an optimal
bin-packing scheduler. A callback only receives borrowed IDs and may stop the
walk by returning a nonzero status.

`mulib_buildplan_critical_path` sums caller-supplied non-negative finite duration
estimates and returns one maximum-duration dependency path. It does not profile
work or predict durations.

## Limits and errors

Construction and planning use checked allocations and fixed graph capacities.
Status values distinguish invalid input, allocation failure, exhausted capacity,
cycles, overflow and mutation during traversal. Objects require external
synchronization. Destroying a plan during a callback is ignored.

## Example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
  examples/modules/buildplan.c -o /tmp/opencode/buildplan
/tmp/opencode/buildplan
```

`tests/test_buildsys_kernels.c` covers merged declarations, deterministic order,
cycles, resource waves, busy callbacks and critical paths.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-buildplan.h`](../kernels/mulib-buildplan.h) |
| Implementation switch | `MULIB_IMPL_BUILDPLAN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/buildplan.c`](../examples/modules/buildplan.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/buildplan.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
