# `mulib-unidiff.h`

**Macro:** `MULIB_IMPL_UNIDIFF` in one C11 TU. No external dependencies.

`mulib_ud_parse(text, length, limits, &patch, &error)` parses a bounded standard
unified diff. The patch owns a byte-for-byte source copy, file list, hunk arrays,
old/new path strings and borrowed hunk text spans into that source. Initialize
with `{0}` or `mulib_ud_init`; free with `mulib_ud_free`.

- Supports multiple `---` / `+++` file blocks and `@@ -start,count +start,count
  @@` hunks; omitted count means 1.
- Accepts `diff --git`, `index`, and new/deleted file mode lines as descriptive
  metadata. The `---` and `+++` paths control creation/deletion via `/dev/null`;
  metadata does not set file permissions. Existing-file mode changes are
  unsupported.
- Supports marker `\\ No newline at end of file` and exact line ending bytes.
  A trailing CR in a patch content line is preserved, allowing CRLF matches.
- Rejects absolute/traversal paths, path renames, binary data patches, combined
  diffs, and malformed hunk lengths.
- Does not support fuzzy matching, context shifting, abbreviated/git-specific
  escape decoding or application against arbitrary prefixes. Quoted names and
  tabs in file headers (including tab-separated timestamps) are rejected rather
  than reinterpreted as shorter paths.

`mulib_ud_apply(&file, base, bytes, max_output, &blob)` checks original line
positions, context, deletion bytes, line endings and range accounting. On
success it produces a malloc-owned exact output. It does not touch disk.

Default parse limits (when options is NULL): 16 MiB patch, 128 files, 4096
hunks, 500,000 hunk lines, 64 MiB target. `max_output_bytes` is provided to
`apply` independently. For a wrong preimage the result is `MULIB_UD_CONFLICT`.

Integration: call `mulib_we_apply_unidiff()` for a workspace-checked batch; it
checks actual files and does not execute metadata or commands.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-unidiff.h`](../include/mulib-unidiff.h) |
| Implementation switch | `MULIB_IMPL_UNIDIFF` |
| Repository-local includes | None |
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
