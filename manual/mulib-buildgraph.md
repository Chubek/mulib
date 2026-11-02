# MuLIB buildgraph

**Header:** `include/mulib-buildgraph.h`  
**Switches:** `MULIB_IMPL_ARTIFACT`, `MULIB_IMPL_BUILDGRAPH`

The graph is a bounded, bipartite dependency DAG. Every node is either an
artifact or a borrowed opaque `struct Action`. MuLIB never defines that struct;
applications can implement commands, compiler invocations or remote actions in
their own executor.

## Construction and hermetic contract

`mulib_buildgraph_create(nodes, edges)` allocates fixed capacities; zero capacities
are valid. Adding a node returns a stable zero-based ID. Names are unique,
canonical relative slash paths, limited to 4095 bytes: absolute paths, empty
components, `.`/`..`, backslashes, colons and control bytes are rejected. Names
describe bindings, not automatically materialized host paths.

`mulib_buildgraph_artifact` retains a supplied source artifact. NULL creates a
generated placeholder. `mulib_buildgraph_action` borrows a non-NULL Action.
Dependencies have exactly two forms:

```
artifact -> action       declared input
action   -> artifact     declared output
```

A generated artifact must have exactly one producer; a source cannot have one.
Duplicate edges are idempotent. Artifact-to-artifact and action-to-action edges
are invalid. Declare tools, configuration and other influences as input artifacts.
The complete graph is validated before either walker invokes a callback, even
when walking a target subset. Missing producers and cycles fail without executing
or translating any nodes.

Hermeticity is the execution interface's **declared-capability contract**. An
executor receives only the Action and its explicitly declared artifact bindings.
It must not consult undeclared host files, inherited environment, clock or network
state. Opaque objects and external references must identify immutable inputs.
The library does not intercept arbitrary C callbacks or supply an OS sandbox.
A process/remote backend is responsible for staging declared inputs and enforcing
filesystem, environment and network isolation. Graph names alone do not make a
host process isolated.

## Execute walker and output helpers

```c
int execute(void *user, const struct Action *action,
            mulib_buildgraph_step_t *step);
```

`mulib_buildgraph_execute` accepts targets and an executor. A zero target count
selects the whole graph. Otherwise it visits the prerequisite closure, including
all outputs of selected actions, without running consumers that are not needed.
Each selected node is visited once in deterministic topological order, with FIFO
insertion-order tie breaking. Execution is serial.

`mulib_buildgraph_inputs` and `outputs` expose borrowed binding arrays in edge
insertion order. Each binding includes the node ID and logical name. Input values
are available only after successful producers. `mulib_buildgraph_output(step,
slot, artifact)` retains/replaces one staged output. Callers may immediately
release their own reference afterward. All output slots must be assigned, and the
executor must return zero, before any staged output is committed. Failure releases
all staged references; it does not undo external side effects performed by an
executor. Missing outputs record `MULIB_BUILDGRAPH_MISSING` on the action.

Independent branches continue after failures; dependent artifacts and actions
become BLOCKED. The owned run report is returned for OK and FAILED, and contains
per-node PENDING (unselected), SUCCEEDED, ERRORED or BLOCKED states. `result`
retrieves states and original callback codes; `value` borrows a successful source
or produced artifact. A report retains its values independently of the graph and
survives graph destruction. Release it with `mulib_buildgraph_run_destroy`.

Each run starts afresh. There is no implicit cache, hidden host filesystem lookup,
timestamp freshness policy, or persistent output directory. Backends can publish
successful report artifacts to their own storage using the generic artifact view.

## Translate walker

`mulib_buildgraph_translate` takes the same target selection plus `node` and
optional `edge` callbacks. For each selected node it calls `node`, followed by its
incoming edges. Both endpoints have already been announced when `edge` runs.
Actions are never executed and generated artifacts remain placeholders. This is
suitable for constructing another graph, a build-system IR, or a rendered plan.
Nonzero translator returns stop the walk and propagate unchanged. A streaming
translator may already have emitted a prefix on callback failure.

## Lifetime and errors

Graph destruction releases names and sources, never Actions. Actions and callback
contexts must remain alive during traversal. Binding arrays/strings must not be
retained beyond their callback. Do not destroy a graph/runtime from a callback;
graph destruction while walking is ignored. Mutation and nested walkers return
BUSY. Objects require external synchronization; callbacks must return normally.

API errors are negative `MULIB_BUILDGRAPH_*` values. Out IDs change only on
success. `execute` requires `*out == NULL`; validation/allocation failures leave
it NULL and do not call the executor. Capacities are overflow checked. Construction
returns NULL for invalid capacities or allocation failure. Validation uses linear
scratch storage; this focused implementation scans edge lists when ordering nodes
and is intended for bounded graphs, rather than million-node scheduling.

## Example and verification

```sh
cc -std=c11 -Iinclude examples/modules/buildgraph.c -o /tmp/opencode/buildgraph
/tmp/opencode/buildgraph
```

`tests/test_buildgraph.c` covers both walkers, target closure and sibling outputs,
duplicate/invalid edges, cycles, missing producers, failed/missing outputs,
descendant blocking, report ownership, allocation failures and empty graphs.
