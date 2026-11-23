# `mulib-workspace-edit.h`

**Macro:** `MULIB_IMPL_WORKSPACE_EDIT`, plus separate implementation switches
for `workspace-fs` and `unidiff` in exactly one translation unit.

`mulib_we_apply(workspace, changes, count, limits, report)` accepts a list of
`PUT` / `DELETE` changes. For each one, `expected_present` is mandatory:

- `0`: the file must not exist (create-only PUT).
- `1`: the regular file must exist with the **exact provided bytes**.

The kernel rejects duplicate paths, unsafe paths and attempts to alter the
root `.mulib-we.lock` file. It takes an advisory workspace-level flock,
reads/confirms all preimages, stages PUT files, creates hard-link backups for
existing files, checks preimages again, and commits files one-by-one with parent
`fsync`. After success it removes backups. On an ordinary commit error it
attempts reverse-order rollback and reports any failed restores.

`mulib_we_apply_unidiff` transforms a parsed `mulib_ud_patch_t` into the same
PUT/DELETE change list using the actual file bytes as the preimage. It
performs strictly checked exact-hunk application, **not** a shell/git apply.

`mulib_we_apply_audited` accepts an optional synchronous callback reporting
`STAGED`, `COMMITTED`, `ROLLED_BACK` and `FAILED` events. These events carry
file paths, byte counts, index and status; not file contents or credentials.
They are informational, not an authorization grant or durable audit log. The
caller must persist them in trusted storage asynchronously or through an
already-allocated, non-blocking queue. Do not reenter this API from callbacks.

## Limits, crash behavior and concurrency

Default limits: 128 files, 16 MiB per file, 64 MiB across expected and new
payloads. The workspace lock coordinates **participating** MuLIB clients; it
does not stop other processes from replacing files. Ordinary failures trigger
rollback, but multi-file updates are not a single atomic rename. They are not
crash-atomic, do not have a durable WAL/recovery service, and can expose partial
states to observers during commit/rollback. For crash consistency use a
higher-level content-addressed workspace generation, snapshot filesystem or
journaled transaction supervisor.

If a filesystem sync or rollback fails, `MULIB_WFS_UNCERTAIN` means the host
must inspect state; do not claim edits fully committed or fully restored.
When rollback fails, hard-link backups are intentionally retained with
`.mulib-wfs-*` basenames; an audit failure event includes the backup basename
for an operator to recover. The host should preserve the audit event and avoid
blindly deleting this file.

`MULIB_WE_TEST_FAIL_COMMIT_AT=N` is an **optional test-only** compile-time
fault injection constant. Never enable it in a production link product.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-workspace-edit.h`](../include/mulib-workspace-edit.h) |
| Implementation switch | `MULIB_IMPL_WORKSPACE_EDIT` |
| Repository-local includes | [mulib-unidiff](mulib-unidiff.md), [mulib-workspace-fs](mulib-workspace-fs.md) |
| Complete example | [`examples/apply_unidiff.c`](../examples/apply_unidiff.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/apply_unidiff.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
