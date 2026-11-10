# ingest-stream — bounded newline accumulation

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_INGEST_STREAM`.

Init zero state with 0 < max_record < SIZE_MAX; reinitializing live state fails.
Push accepts binary chunks, buffering bytes until LF. Callback borrows a record
excluding LF and one preceding CR; empty lines are emitted. max_record counts
buffered bytes, including CR. Push returns emitted records or SIZE_MAX on invalid
input, allocation/length limit, or callback rejection (zero).

Push is **incremental**, not transactional: prior callbacks and buffered bytes
remain committed on failure, and the API does not report a consumed-byte offset.
Do not blindly replay a failed chunk. A rejected record remains buffered; callers
can retry it with finish or reset/free the stream. Finish emits a nonempty final
unterminated buffer (stripping a final CR), returning 1, 0 when empty, or SIZE_MAX;
rejection preserves the buffer. Finish does not seal the stream. Free discards
pending bytes. O(chunk bytes), memory bounded by max_record.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ingest-stream.h`](../include/mulib-ingest-stream.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Bounded chunk accumulation and newline record streaming with callback. C11
 * single-header; define MULIB_IMPL_INGEST_STREAM once. */
#ifndef MULIB_INGEST_STREAM_H
#define MULIB_INGEST_STREAM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    unsigned char *data;
    size_t length, capacity, max_record;
  } mulib_ingest_stream_t;
  typedef int (*mulib_ingest_record_fn) (const void *bytes, size_t length, void *ctx);
  int mulib_ingest_stream_init (mulib_ingest_stream_t *s, size_t max_record);
  /* Return processed record count, SIZE_MAX on invalid/callback failure. */
  size_t mulib_ingest_stream_push (mulib_ingest_stream_t *s, const void *data,
                                   size_t length, mulib_ingest_record_fn fn, void *ctx);
  /* Emit a nonempty final unterminated record; 1 emitted, 0 empty, SIZE_MAX error.
   * A rejected record stays buffered for retry. Initialize from zero, free to reuse. */
  size_t mulib_ingest_stream_finish (mulib_ingest_stream_t *s,
                                     mulib_ingest_record_fn fn, void *ctx);
  void mulib_ingest_stream_free (mulib_ingest_stream_t *s);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ingest-stream.h`](../include/mulib-ingest-stream.h) |
| Implementation switch | `MULIB_IMPL_INGEST_STREAM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ingest-stream.c`](../examples/modules/ingest-stream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ingest-stream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
