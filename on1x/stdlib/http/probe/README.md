# `std.http.probe` execution

This package composes an Elvi **document-capture** profile, canonical
[`std.html`](../../html/README.md), and application On1x callbacks. It does
not interpret the legacy Elvi Lua/XPath/IPC workflow. A profile using Elvi's
`artifacts discard` cannot expose a document through Probe. See
[`SPEC.md`](SPEC.md) for the distinct Probe result-artifact policy.

## One shot

| Operation | Result |
| --- | --- |
| `basis_http_probe_make(spec, parse)` | Validated `Probe<T>`; does not run the parser. |
| `basis_http_probe_run(probe, host)` | `Result<ProbeRun<T>, ProbeRunError>`; no result handler. |
| `basis_http_probe_run_with(probe, host, handler)` | The same result; delivers `ProbeResult<T>` once, synchronously, on success. |
| `probe::basis_http_probe_run(host, handler)` | `Result<BasisHtml, ProbeRunError>`; invokes the same success handler. |
| `probe::basis_http_probe_run_result(host, handler)` | `Result<ProbeResult<T>, ProbeRunError>`, including metadata and parsed value. |

Construct a `ProbeHost` with an explicit `authorize: fn(ProbeRequest) -> bool`
and a `FetchBackend` with
`capture: fn(ElviProgram, ProbeRequest) -> Result<BasisElviDocument, i64>`.
`ProbeRequest` is the Elvi capture request (`url`, `host`, `max_body_bytes`).
The approved URL is exactly the program's compiled `home`. A `ProbeHandler<C,T>`
has `context: C` and `deliver: fn(ProbeResult<T>, C) -> void`. Callbacks may
be named functions or capture local values; they are evaluated on the caller's
thread and never carried across an FFI or process boundary. The context and
callbacks are not a durable job format.

Execution revalidates the possibly directly constructed `Probe<T>` before
consulting the host. Unsupported modes and cookie stages are rejected before
authorization, fetch, file access, or parser invocation. After one explicit
approval, [`basis_elvi_capture`](../../elvi/capture.md) invokes the chosen
backend at most once and checks the returned metadata, exact URL and body
bound. Probe then accepts only a 2xx response with ASCII-case-insensitive
`text/html` and no parameters or one `charset=utf-8` parameter. There is no
transcoding, sniffing, redirect following, retry, fallback engine, or
implicit file/IPC sink. The shared 64 KiB decoded input limit does not
override the tighter limits for normalized markup and other HTML resources.
NUL and malformed UTF-8 are rejected by `std.html` before the parser runs.
Canonical HTML is not a sanitizer.

The parser receives canonical HTML exactly once on eligible input. A
`ProbeResult<T>` always contains the owned canonical `document`, structured
metadata, and parser-generated `value`. Its optional artifact fields are
separate: `raw` is exact lowercase hex only when requested, and `html` is a
normalized artifact only when requested. Discard omits both optional
artifacts, not the canonical document used by the caller and success
handler. It is not erasure, sandboxing, or a prohibition on the application's
retention. Neither a result nor a result callback is produced on failure.
`run::basis_http_probe_run_document()` projects the same canonical document
from the one-shot receipt.

Transfer and atomic-save policies fail before network authorization if no
`ProbeArtifactSink` is supplied. `basis_http_probe_run_artifact_with` checks
the destination key through `sink.authorize` before fetching; only after
normalization and parser success does it call `sink.write` once. A rejected
or uncertain write keeps the handler from running. Only a confirmed commit
is success; writes with uncertain durability are not retried. The sink must
honor the exact requested action; transfer is deployment-owned. For a
write-once native atomic-save adapter, see [`ARTIFACT.md`](ARTIFACT.md).
There is no default path, sink, or transfer destination. Errors do not roll
back effects of callbacks already entered; in-process callbacks cannot be
interrupted by Probe.

`basis_http_probe_run_targets` handles a `ProbeTargets<T>` plan of 1–8
targets, sequentially and in index order, with an explicit `at(index)`
supplier. Each target has its own specification and approval and delivers
one successful result before the next target starts. It returns the count
on success or the index and cause of the first failure. A bad plan count
fails before any supplier call, and a missing target stops the batch. Prior
effects are not undone. There is no automatic concurrency, deduplication,
or batch retry; effect policies requiring a sink use the one-shot entry.

### Transport and authority

The default import starts no network operation and includes no implicit live
backend. For a socket-free example that actually drives the Elvi HTTP
capture and HTML engines with fixture bytes, see
[`on1x/examples/http-probe`](../../../examples/http-probe). The fixture's
simulated peer report is **not** evidence of live TLS verification.

`std.http.probe.live.basis_http_probe_live_backend()` is an explicit adapter
to `std.elvi.live`. Link the Elvi live and capture companions and implement
the strong `on1x_elvi_live_authorize_v1` host symbol. This is in addition to
`ProbeHost.authorize`: neither API treats DSL text or a nominal metadata
record as authority. Production hosts must independently enforce egress
(including the actual destination at connect time), verified certificate
chain and hostname, authenticated shutdown, and request, rate, policy and
deadline budgets. Probe ships no production socket/TLS provider, DNS, robots
or credential policy, and no live-integration claims are made on the basis
of test peer reports. See [`elvi/live.h`](../../elvi/live.h) for the C ABI and
[`elvi/capture.md`](../../elvi/capture.md) for transport and limits.

## Native supervision and durable dispatch

[`supervised.h`](supervised.h) is a separate trusted C11 composition
interface, not an On1x foreign handle ABI. It uses the public
[`std.daemon` supervisor](../../daemon/README.md) and
[`std.kron` runtime](../../kron/runtime.h). No owners, process IDs, code or
closures are serialized. The application supplies a trusted daemon
allowlist, live supervisor owner, current launch policy and bounded timeouts.

- `basis_http_probe_run_supervised` validates the descriptor, runner binding
  and limits, obtains approval for this attempt, and runs the selected
  foreground child. `JOB_OK` requires a reaped exit code of zero. Inspect the
  daemon status/event for setup errors, timeouts, nonzero exits and signals.
  When `has_daemon_event` is true, inspect `child_owned`; if it remains true,
  retain the owner and continue supervising.
- `basis_http_probe_daemonize` performs the same authorization and starts a
  child with `basis_daemon_start`, then returns a caller-owned handle at exec
  acknowledgement (not service readiness). Use `basis_daemon_poll` or
  `basis_daemon_wait` for status, `basis_daemon_stop` for cancel/shutdown, and
  destroy only after reap. Retain any returned handle even on error.
  An asynchronous caller must drive its own run deadline (`wait` then `stop`
  at the deadline), or use a watchdog; there is no implicit background
  monitor. `run_supervised` and scheduled dispatch enforce the daemon
  run/stop/reap budgets while waiting.
- `basis_http_probe_schedule` stores a versioned, canonical byte descriptor
  via Kron. The descriptor has three case-sensitive registry keys (probe,
  runner, context), each 1–63 ASCII characters, at most 196 bytes total.
  Scheduling validates the schedule and all keys, then requires an explicit
  admission callback to check the named runner, immutable target definition,
  and context are approved for persistence. That does not transfer code,
  grant future authority, reserve egress budget or persist credentials,
  paths, URLs, artifacts or policy. Use a dedicated trusted Kron directory
  and stable, versioned registry identities; don't reassign a pending key
  to a different application job. The returned promise ID is a Kron value;
  use Kron's pull, signal and next operations for queue management.
- `basis_http_probe_dispatch` asks Kron to consume at most one due occurrence
  using the caller's clock. After a confirmed consume/rearm, Probe decodes the
  descriptor, validates the current runner binding, and asks the current
  host to authorize that `(id, deadline, keys)` attempt. Only exact approval
  starts the child. Denial or a malformed descriptor consumes that
  occurrence without launching; denial does not automatically cancel a
  recurring promise. No auth or launch callback is invoked on idle, expiry,
  pre-commit error or uncertain durability.
- The child uses `basis_http_probe_decode_child` with its deployment's
  expected runner name to validate both the daemon envelope and Probe
  descriptor. It resolves the keys through its own trusted registry and
  independently authorizes any I/O. Successful decoding is not proof of
  authority: argv can be forged and inspected. The supervisor returns only
  process status; it does not forward result bodies or manufacture a
  cross-process handler. A runner requiring a durable result must arrange its
  own authorized sink and recovery semantics.

The native path has no overload accepting a function, captured closure,
On1x owner, or arbitrary context payload. A named handler or a registered
closure implementation and its admitted context must be rebuilt inside the
runner by trusted code from registry keys. Do not encode captured state or
secrets in those keys. Invalid keys are rejected as invalid descriptor data
and a denied or unresolved registration must be rejected at admission and
again at dispatch.

**Crash/retry:** Kron consumes or rearms before launch. A crash after the
commit can lose that occurrence, even before the child starts; it can also
leave effects that have not been reported back. No exactly-once or automatic
retry guarantee is made. A `*_UNCERTAIN` result is not a clean failure; do
not blindly delegate or dispatch the same work again. Reopen and reconcile
using the durable state and application-owned effect records. If retry is
appropriate, agree on idempotency using stable occurrence identity or a
trusted application key. Kron and daemon do not jointly commit artifacts.
Only the direct child is supervised during the host's lifetime; use an
external supervisor when crash cleanup or descendant isolation is required.

## Verification

Use a current-source C11-capable On1x compiler; older compilers reject
immutable nominal text and closure fields. No Probe wrapper adds kernel
implementation switches or claims reviewed safe foreign admission. Use a
single MuLIB implementation translation unit for all linked dependencies.

```sh
python3 -B on1x/stdlib/http/probe/tests/run.py \
  --on1xc build/on1x-probe/on1xc --cc clang
```

On Linux this builds and runs real `on1xc` C11 output, checks callback
ordering, per-policy artifacts and short-circuit failure, checks codec bounds,
and exercises real child launches and Kron reopen/replay/denial. It also
links the optional live adapter against Elvi's socket-free wire fixture;
it does not contact a public network or qualify a production TLS provider.
