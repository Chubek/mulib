# Explicit HTTPS document capture from On1x

`std.elvi.live` is an optional native C11 profile. The ordinary `std.elvi`
import does not enable it. Call `basis_elvi_live_fetch(program, url, limit)`
with an Elvi `mode document` / `artifacts capture` program and a decoded-body
ceiling in `1..65536`. An empty URL selects the program's home; explicit URLs
must pass the program's exact-host policy. The bridge recompiles the source
and checks the requested URL before asking the host for authority.

Link `live.c`, `capture.c`, `bridge.c`, `runtime.c`, and the normal On1x
runtime. Provide the Elvi kernel closure once, for example via `elvi/kernels.c`
and `-lm`. The trusted host must implement the strong
[`on1x_elvi_live_authorize_v1`](live.h) link symbol. Do not replace it with an
allow-all policy. It must independently authorize the actual URL, TLS trust,
resolved address, optional cookie source, and budgets. Its transport callback
must enforce destination policy again at connect time and actually verify
the peer. The bridge does not open a default trust store or choose a TLS backend.
There is one GET without redirect, retry, or decompression. Apply robots rules,
rate and concurrency limits, and a total wall-clock deadline in the trusted
host or a separately supervised process. This synchronous entry point is not
a sandbox and cannot interrupt a blocked host callback.

`Ok` carries an immutable On1x snapshot with a reversible lowercase-hex body
and checked Elvi HTTP/TLS metadata. No native storage is borrowed by the result.
This profile does not expose arbitrary headers, credential contents, or
timestamps. It cannot retrieve the body of a legacy Lua/IPC `artifacts discard`
run. The runtime text snapshot pool is finite and process-scoped; do not treat
this API as incremental streaming or run an unbounded fetch loop in one process.

| `Err` | Meaning |
| --- | --- |
| `-1` | Invalid program profile, URL argument, or limit |
| `-2` | Local URL policy or trusted host denied the request |
| `-3` | Independently supplied cookie authority/credentials were rejected |
| `-4` | TLS peer or negotiation failure |
| `-5` | Transport failure |
| `-6` | Malformed or unsupported response |
| `-7` | Redirect not followed |
| `-8` | Response limit exceeded |
| `-9` | Native allocation failed |
| `-10` | Inconsistent bridge result |

On1x runtime text-allocation errors use the existing fatal path, not `-9`.
The only new raw extern is `on1x_elvi_live_snapshot_v1`; it accepts borrowed
NUL-free input strings and returns an immutable runtime-owned text snapshot
or static error. The generated-C binding is explicit, not a reviewed safe
foreign admission.
