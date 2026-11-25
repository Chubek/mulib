# MuLIB event scheduler

Include `mulib-evtsched.h` and define `MULIB_IMPL_EVTSCHED` in one translation
unit. The queue and deterministic dispatch use C11 and have no MuLIB dependencies.
The blocking runner uses Linux realtime timerfd. A runnable, simulated-clock
example is in [evtsched.c](../examples/modules/evtsched.c).

## Queue and ownership

Zero-initialize a scheduler and each notice before first use. Keep their addresses
stable and serialize access. Notices and their optional `data` payloads belong to
the caller. Cancel before destroying a queued notice, or clear the scheduler
before destroying its notices. Clear detaches every notice and releases all
allocated bucket metadata. It is safe on an empty scheduler and permits reuse.

Schedule takes an absolute signed 64-bit deadline in seconds. Scheduling an
already queued notice moves it to the end of its new deadline's FIFO group.
Hold postpones relative to the notice's current deadline. A zero delay preserves
tie position. Negative delays and attempts to hold or cancel a detached notice
fail with EINVAL. Overflow fails with EOVERFLOW. Foreign-queue operations fail
with EINVAL and leave the notice in its original queue. Peek borrows the minimum;
pop transfers it back detached. Both return NULL on an empty queue.

Ordered buckets contain intrusive doubly linked notice lists. Above 32 entries
a bucket attempts to split at its median, retaining order across equal deadlines.
The first bucket is embedded. If a split allocation fails the insertion still
succeeds in the original bucket. Empty dynamic buckets are freed immediately.
Peek, pop and cancellation are constant time. Insertion scans bucket boundaries
and the selected bucket and is linear in the worst case.

## Dispatch and recurrence

Dispatch takes an explicit current time, callback, and context. It returns 1
after one due callback or 0 when nothing is due. The notice is detached before
the call. The callback may release it, cancel other notices, or schedule another
occurrence using an absolute timestamp. The caller chooses whether missed
periods catch up or skip ahead. No clock or cron policy is built into the queue.

On Linux, call wait_one repeatedly to run until empty. Each call checks wall
time, arms a close-on-exec absolute realtime timer for a future minimum, retries
interrupted reads, and rechecks the time before dispatch. System failures
return -1 with errno and leave the notice queued. A future deadline must fit the
platform time_t and native timer range. Other platforms support explicit-time
dispatch and return ENOSYS for a nonempty wait. A callback that perpetually
reschedules itself can keep a caller's loop running indefinitely.

## Provenance

Adapted from the scheduling design of Lykron (`.sot/lykron/scheduler.c`).
The upstream license follows.

Copyright 2025 Chubak Bidpaa <chubakbidpaa@riseup.net>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-evtsched.h`](../kernels/mulib-evtsched.h) |
| Implementation switch | `MULIB_IMPL_EVTSCHED` |
| Repository-local includes | None |
| Complete example | [`examples/modules/evtsched.c`](../examples/modules/evtsched.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/evtsched.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
