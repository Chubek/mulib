# dbquery

`scan` visits live records in unsigned-byte key order over [low,high), with a
callback limit. NULL high/zero length means no upper bound; an actual empty
upper key requires a non-NULL pointer. NULL low/zero length denotes the empty key.
Tombstones are skipped, limit zero visits none. Empty/reversed ranges visit none.

Callback borrows each record and returns nonzero to continue; never mutate/free
the table during a scan. Returned count includes a stopping callback; SIZE_MAX
means invalid arguments. No expression planner or secondary-index selection.
Switches: `MULIB_IMPL_DBQUERY`, `MULIB_IMPL_MEMTABLE`.
Example: `examples/modules/dbquery.c`.
