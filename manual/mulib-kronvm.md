# KronVM

KronVM turns one KronQL statement into an identified promise. An embedding
application supplies Unix time in seconds, asks for the next deadline, and
calls dispatch when ready. Each dispatch consumes at most one occurrence,
runs its command synchronously and reaps the child. Equal deadlines run in
enqueue order, including after reopening the cache.

## KronQL

~~~text
statement = action schedule [lifetime] ";"
action    = "DELEGATE" backtick-shell-command | "EXECUTE" double-quoted-absolute-path
schedule  = "ONCE" "AT" absolute | "EVERY" duration
lifetime  = "PERSISTENT" | "FOR" duration | "UNTIL" "SIGNAL" "(" positive-integer ")"
absolute  = "<" unix-seconds ">" | "<" hour ":" minute ":" second ";" month day ">"
duration  = "<" positive-integer ("s" | "m" | "h" | "d") ">"
~~~

Keywords and three-letter English month names are case-sensitive. Numbers
are unsigned decimal. Whitespace separates words and may surround punctuation.
A duration's digits and unit are adjacent. Quoted payloads are literal bytes
up to their closing delimiter and must be nonempty. KronQL has no delimiter
escape or variable interpolation. The shell performs its usual expansion
inside DELEGATE. EXECUTE passes the given path as argv[0] with no arguments.

~~~text
DELEGATE `echo foobar` ONCE AT <12:30:00; Jan 24>;
EXECUTE "/usr/bin/true" EVERY <1h> UNTIL SIGNAL(9);
DELEGATE `echo tick` EVERY <5s> FOR <20s>;
~~~

A yearless date selects the next matching UTC instant on or after submission,
through year 9999. February 29 advances to a leap year. A past epoch deadline
is immediately eligible. EVERY first fires one interval after admission.
After a late dispatch, the next deadline is the first strictly future point
on the original cadence. Missed periods are coalesced into one launch.
Recurrence ends when the next deadline exceeds the signed 64-bit clock range.

FOR bounds the lifetime from admission, inclusively. It rejects a first
deadline beyond that limit. Dispatch after expiry retires the promise without
starting a child. PERSISTENT is the default. ONCE still retires after one
occurrence. All pending promises, including temporary ones, survive close.
Pull cancels by ID. Signal cancels every matching pending promise in one
transaction. Signal numbers are application events. Deliver them from the
host's normal event loop, outside any asynchronous signal handler.

## Embedding and recovery

Compile the implementations as C11 on a POSIX system providing flock. C++ callers can include the declarations and link the C objects. Define MULIB_IMPL_KRONVM,
MULIB_IMPL_EVTSCHED, MULIB_IMPL_KVDB, MULIB_IMPL_DATE, MULIB_IMPL_ENDIAN,
MULIB_IMPL_BUFFER, MULIB_IMPL_VARINT and MULIB_IMPL_CRC32 once in the
application. Serialize access to each VM. Open accepts NULL for
`/var/run/kronvm` or an absolute directory path. The parent must exist
and permit creating the directory. Use a trusted directory dedicated to
this VM, owned by the effective user and writable only by that user. One live VM owns the directory lock. The cache is
`events.kvdb`, an atomic kvdb snapshot. Runtime directories may be cleared
on system reboot. Commands run with the host's environment and privileges.

Delegate returns a stable ID through its output pointer. Pull, signal and
delegate return zero for a committed change, one when the replacement is
visible but the directory sync failed, or minus one with errno on failure.
Next returns one and writes the earliest deadline, or zero for an empty queue.
Dispatch returns zero when idle, one after consuming an occurrence, two when
consumed with uncertain durability, or minus one before consumption. On idle
or failure the result is untouched. A consumed result identifies the promise
and original due time. A launched child supplies a waitpid status; use
WIFEXITED and WEXITSTATUS or WIFSIGNALED and WTERMSIG. A failed exec exits
127. Fork or wait failures set execution_errno. Expiry leaves launched and
execution_errno zero.

KronVM commits removal or advancement before starting a child. A process crash
in that gap can skip a launch. A command failure consumes that occurrence.
Under normal successful commits, reopening restores only the remaining work.
Each mutation reads the snapshot and commits changed records before updating
the in-memory queue. Budget I/O proportional to the cached schedule size.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-kronvm.h`](../kernels/mulib-kronvm.h) |
| Implementation switch | `MULIB_IMPL_KRONVM` |
| Repository-local includes | [mulib-date](mulib-date.md), [mulib-endian](mulib-endian.md), [mulib-evtsched](mulib-evtsched.md), [mulib-kvdb](mulib-kvdb.md) |
| Complete example | [`examples/modules/kronvm.c`](../examples/modules/kronvm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/kronvm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
