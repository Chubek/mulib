# Web worker staging primitives

These are bounded, synchronous host helpers, not a browser worker runtime.
They use the C11 static-inline profile: no `MULIB_IMPL_*` switch, external ABI,
heap allocation, thread synchronization, network transport or script execution.
Each header includes the staging detail helpers; `web-sharedworker` also
includes `web-storagekey`. Neither URL/origin parsing nor P0 runtime contracts
are needed by this slice.

## Contracts

- `web-worker`: Zero-initialize state, then supply a start function and optional
  stop function and context. `start` runs only in `NEW`, returning the callback's
  result verbatim. Zero leaves `NEW` for retry; any nonzero value publishes
  `RUNNING`. Reentrant start and terminate calls during start return zero.
  `terminate` returns 1 for a transition from `NEW` or `RUNNING` to terminal
  `STOPPED`, otherwise zero. Canceling `NEW` does not call stop. Terminating
  `RUNNING` publishes `STOPPED` first, then calls stop at most once, so reentry
  cannot dispatch again. Callbacks must not alter, reinitialize or release the
  worker during the operation. The host is responsible for cleaning up a failed
  start and for actually stopping work; neither can be inferred from state.
- `web-serviceworker`: Supply a table of scopes and a URL already serialized and
  validated by the host. Matching considers the URL before its first literal
  `#`; percent-encoded bytes remain literal. Among nonempty matching prefixes
  with the supplied nonzero origin ID and a nonzero worker handle, the longest
  wins; ties use table order. Zero means no match. A prefix need not end on a
  path-component boundary. This does not parse URLs, authorize scope, derive
  origins or storage partitions, or verify that a worker is active.
- `web-worklet`: Dispatch by an exact nonempty name, including any embedded NUL
  bytes. Consult the whole name table before dispatching: a duplicate matching
  name or a matching entry with no function prevents any callback. Return the
  one matched callback's result unchanged, or zero if not dispatched; zero
  from a callback is not distinguishable by the return value alone. The caller
  owns the context and any recursion policy for this stateless helper.
- `web-locks`: A shared request is compatible with other shared holds of the
  same name; an exclusive request or hold conflicts with any matching hold.
  Names are exact byte spans, including empty names. A malformed request or
  held-lock span (`NULL` bytes with nonzero length) yields unavailable, as does
  a missing nonempty snapshot. The `owner` field is metadata, not a reentry
  exemption. A positive result is not a grant and does not reserve the lock.

All tables and spans are borrowed. Counts must reflect actual storage, and
nonempty spans must point to accessible bytes. State and borrowed callback
contexts must remain valid throughout the call. No check supplies isolation or
authorization, and a callback's side effects cannot be rolled back.

## Other worker ledger rows

| Header | Audited, unchanged behavior | Not provided |
| --- | --- | --- |
| `web-sharedworker` | Exact storage-key, URL and name comparison. Storage-key equality compares origin/site bytes and nonce. | Origin or storage-key derivation, admission, ports and reuse lifecycle. |
| `web-workerclient` | First active client whose ID and origin ID match, as a borrowed pointer. | Client creation, navigation, focus and message delivery. |
| `web-workerglobal` | Requires a nonzero realm, a non-closing context and overlap with **any** bit of the exposure mask. | JS globals, event targets, timers, Fetch or script loading. |
| `web-taskqueue` | Tries initialized FIFO sources round-robin, runs at most one callback per step, and rejects nested steps on the same scheduler. | HTML runnable-task rules, microtask checkpoints, priorities and threaded scheduling. |
| `web-agentcluster` | True only for equal nonzero cluster IDs and both isolation flags set. | Agent creation, verification of isolation and shared-memory model enforcement. |

The slice likewise does not provide worker script fetching or threading,
service-worker install/update/activate or Fetch interception, worklet realms or
realtime scheduling, or lock queues, abort, fairness or JS promises.

## Offline verification

Run the [worker example](../examples/modules/web-worker.c) and
[focused worker tests](../tests/test_web_workers.c) from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
    examples/modules/web-worker.c -o worker-example
./worker-example
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
    tests/test_web_workers.c -o test-web-workers
./test-web-workers
```
