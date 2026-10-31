# mulib-container-log

## NAME AND INTEGRATION

Bounded binary log ring with sequence cursors, filtering and loss metrics.
Define `MULIB_IMPL_CONTAINER_LOG`; only the inline container core is needed.
Example: `examples/modules/container-log.c`.

## RECORDING

`init(log,records,max_bytes,max_message,overwrite)` allocates a fixed record ring.
records must be nonzero; max_message cannot exceed max_bytes. Each record owns
binary message bytes and a fixed container ID, with sequence, timestamp, level
and application-defined stream. Stream 1/2 conventionally means stdout/stderr.

Levels are TRACE, DEBUG, INFO, WARN, ERROR. Set minimum to suppress lower levels.
Suppressed appends succeed, increment filtered and report sequence zero. Accepted
records receive strictly increasing sequence values starting at one. Exhaustion
returns ELIMIT rather than wrapping. Counters saturate at UINT64_MAX.

`append` validates/copies data before evicting anything. In overwrite mode it
removes oldest records until both record and byte quotas fit, incrementing dropped
for each lost record. Without overwrite, capacity pressure returns ELIMIT and
preserves existing records. Embedded NULs and empty messages are supported.

## CURSORS AND PRUNING

`next(log,&cursor,id_filter,min_level,&record)` returns the first matching retained
record at or beyond cursor. Zero cursor starts at oldest. Success advances cursor
past that sequence. Nonmatching records are skipped permanently by that cursor.
AGAIN means no current match. A cursor older than retained data receives ESTALE
and is moved to the oldest sequence; the caller then retries.

Returned records/messages are borrowed until mutation/free. `prune(before)`
removes records with sequence below before and returns the count; explicit pruning
does not increment overwrite-loss statistics. `free` releases all messages/table.

## OPERATING MODEL

Timestamps are opaque caller values, not automatically sorted. No file, terminal,
background thread or durable journal is opened. Externally serialize readers and
writers; snapshot/copy borrowed bytes before a subsequent append. Allocation and
limit failures use CT errors. Tests cover byte pressure, filtering, stale cursors,
pin-independent binary storage and explicit pruning.
