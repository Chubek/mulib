# mulib-capbroker

## Build
`MULIB_IMPL_CAPBROKER`, `MULIB_IMPL_SANDBOX_PROFILE`; portable C11. Caller-provided
handlers; no OS dependencies. See `examples/modules/capbroker.c`.

`mulib_capbroker_intersect` validates requested and approved names (no duplicate,
max 256), returns only approved matches in requested order, borrowing approved
strings. Output remains unchanged on invalid input or missing capacity.
The supervisor may reject a manifest not receiving all mandatory grants.
`mulib_capbroker_init` borrows a host-owned immutable subject, handler table,
grants, and any hook context. Operation names and grants are exact, case
sensitive, 1..127 bounded ASCII characters. No wildcard or role inference.

For each job/hook, call `begin`, then `call` only after strict transport/schema
validation, then `finish`. `begin` resets the per-job request and combined
input/output payload quotas. Requests/replies have independent frame bounds.
`call` uses its host-fixed identity, looks up an exact operation, verifies its
required grant, charges request bytes, bounds the handler's output capacity,
charges actual reply bytes, and audits the outcome. A missing/denied operation,
invalid input, limit violation, or handler failure is sticky for the job.
A malicious worker cannot recover a successful commit by ignoring denied
requests. Unknown operation is deny, not pass-through.

Transactional hooks are all-or-none (`begin`, `commit`, `abort`). The trusted
handler may stage changes only, with no untracked commits. Begin failure must
clean its own partial initialization. A failed hook/worker rolls back staged
changes with abort. A successful hook calls commit once, preserving
`MULIB_SB_UNCERTAIN` (do not abort or blindly retry). Other commit errors must
represent definite pre-publication failure and are followed by abort. Without
hooks, use pure/read-only handlers or explicitly documented irreversible
operations; this library cannot roll back remote requests. Worker death or a
malformed frame must cause `finish(0)` by the host.

Handlers, names, byte buffers and context must stay valid during a call; never
retain borrowed payloads/buffers or return a borrowed pointer. The host owns
serialization, per-object authority and validation, transactions, deadlines,
logging, and audit persistence. The broker is single-threaded, non-reentrant,
with bounded integer accounting; callbacks must not reenter it. Audit hooks
receive subject, operation and status, not raw payload/secrets.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-capbroker.h`](../include/mulib-capbroker.h) |
| Implementation switch | `MULIB_IMPL_CAPBROKER` |
| Repository-local includes | [mulib-sandbox-profile](mulib-sandbox-profile.md) |
| Complete example | [`examples/modules/capbroker.c`](../examples/modules/capbroker.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/capbroker.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
