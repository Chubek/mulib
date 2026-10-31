# mulib-rootfs-buildspec

## NAME

Owned ordered rootfs recipes with transactional execution and aggregate quotas.

## INTEGRATION

Define `MULIB_IMPL_ROOTFS_BUILDSPEC`, `MULIB_IMPL_ROOTFS`, `MULIB_IMPL_VFS`,
`MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER` once. Includes the inline container core.
Example: `examples/modules/rootfs-buildspec.c`.

## RECIPE MODEL

`init(spec, max_steps, max_bytes, max_result_bytes)` creates an empty recipe.
max_steps must be nonzero. Recipe bytes count copied paths, COPY source paths,
terminators and FILE contents. Result bytes count file payload, not metadata.
Zero byte budgets are real zero limits.

`add` normalizes/copies paths and data before appending one owned step:

| Operation | Meaning |
| --- | --- |
| DIRECTORY | Create the directory and any missing parents. Existing directories succeed. |
| FILE | Replace complete file bytes, creating parents. Binary and empty contents work. |
| REMOVE | Remove an existing entry; recursive optionally removes descendants. |
| COPY | Copy a regular file from the current intermediate filesystem into a destination. |

Non-FILE operations require NULL/zero data. Only COPY accepts source; only REMOVE
accepts recursive. FILE/COPY/REMOVE cannot target `/`. Repeated steps execute in
order, so COPY can read a file produced by an earlier step. Missing REMOVE/COPY
sources are errors. File/directory conflicts propagate native VFS errors.

## ATOMIC APPLICATION

`apply(spec, root, failed_step, detail)` clones root, executes steps, checks total
file bytes after every step, and replaces root only when all succeed. Final
quota checking also occurs for an empty recipe. Temporary copies are released
on failure. failed_step is SIZE_MAX for success/preflight failure or the failing
step index; detail receives the underlying VFS error where applicable.

The root's node/per-file quotas remain in effect. Peak memory includes the old
root, temporary root, recipe data and a COPY scratch file. COPY scratch is bounded
by the result limit. `clone` deep-copies a recipe into an uninitialized output;
`free` releases all steps and resets it.

## RESULTS

Returns common `MULIB_CT_*` errors, with native errors available separately.
Invalid operations/paths are EINVAL, quota failures ELIMIT, copied-storage
allocation failures (including the initial root clone) ENOMEM, other filesystem
operation failures EIO. No host filesystem
operations or process execution occur. See [stack](container-stack.md) for tests.
