# On1x HTTP client

`std.http` provides pure On1x response helpers. It never links network code.
`std.http.client` is an explicit-link HTTP/1.1 GET/HEAD client for trusted
On1x C11 applications with a host networking policy.

```on1x
import std.http as Http;
import std.http.client as Client;

fn main()
{
  let (error, status, headers, body) =
    Client.basis_http_get("https://127.0.0.1:8443/status");
  if error == 0 {
    let (present, kind) = Http.header(headers, "content-type");
    if present { println(kind); }
    println(status);
    println(Http.body_byte(body, 0));
  } else {
    println(error);
  }
}
```

`basis_http_head(url)` returns the same tuple with an empty body.
`Http.success(status)` identifies 2xx statuses. A final non-2xx status is
still a successful exchange; no redirects are followed. `Http.header`
returns `(found, value)` for the first ASCII-case-insensitive name match.
Headers are `Name:value\n` lines, in wire order, with surrounding value
whitespace trimmed by the parser; duplicates are not merged. Empty and absent
values are distinct. Trailers and the reason phrase are not returned.
The body is lowercase hex in a NUL-free string, including for binary results
containing `00` or `ff`. `Http.body_byte` returns `0..255`, or `-1` for an
out-of-range index or an invalid pair. Transfer framing is decoded; content
encodings such as gzip are not decompressed.

| `error` | Meaning |
| --- | --- |
| `0` | Parsed response; inspect `status` (including 3xx/4xx/5xx). |
| `-1` | Invalid input or malformed HTTP response. |
| `-2` | MuLIB HTTP byte or field limit exceeded. |
| `-3` | MuLIB reported an allocation failure. |
| `-4` | Transport failed, including socket wait, TLS setup, peer verification, or missing authenticated TLS shutdown. |
| `-5` | Denied by the host, or invalid/missing grant (including HTTPS trust). |
| `-6` | Inconsistent or unrepresentable bridge reply. |

On error, status is zero and both strings are empty. On1x text allocation
and cumulative snapshot-pool exhaustion use the runtime's fatal error path,
not `-3`. Results are immutable text snapshots retained in the normal On1x
text pool until process exit. Use on the owning On1x runtime thread, not as a
concurrent or cancellable task API.

## Host integration

The only target form is a lowercase `http://` or `https://` URL with
canonical dotted-decimal IPv4, an optional decimal port from 1 to 65535
without leading zeroes, path, and optional query. Defaults are 80/443.
There is no DNS-name, IPv6, proxy, userinfo, fragment, custom request header,
request body, or alternate method API. The bridge makes one connection
attempt for one transaction. It uses `Connection: close` and requires end
of stream (TLS `close_notify` for HTTPS); it does not reuse connections.
Limits are 2,048 URL bytes, 8,192 header bytes, 64 fields, 32,768 decoded body
bytes, and 65,536 HTTP application bytes on receipt, including framing and
informational responses. The HTTP application byte limit does not count TLS
record overhead. `wait_ms` is a bounded **per-socket wait**, not a total
transaction deadline.

Implement `on1x_http_authorize_v1` from [`runtime.h`](runtime.h) in trusted C.
There is no permissive stub: without that symbol, linking the client fails.
Decide before any transport based on the canonical address, effective port,
TLS flag, method, and validated URL. Deny by default and charge a host-owned
attempt budget. Grant only a bounded wait (`1..30000` ms), and for HTTPS supply
an explicit, already-loaded trust store that stays valid throughout the call.
The user cannot choose a trust file or disable verification. Native MuLIB
TLS 1.3 verifies the certificate chain and IP SAN against the approved address
and offers ALPN `http/1.1`. The adapter does not read proxy or CA environment
settings. The numeric-only profile does not authorize DNS names; the MuLIB
socket connector internally uses the platform's address resolver.

**This is not a sandbox or an admitted safe foreign import.** Don't link it
into processes executing untrusted On1x/MuLL, and do not give those programs
access to MuLIB sockets or TLS symbols. A host serving untrusted input should
expose only its own data-only operation, enforce the egress allowlist and
cumulative budgets outside the guest, and run network work in a separately
supervised process with a hard wall-clock deadline. A slow peer can prolong
a transaction by making progress inside each per-socket wait. The package
does not provide that process sandbox/supervisor or modify any existing
host policy. Do not log full URLs or response data by default.

From the repository root, use an existing `on1xc` and a C11 compiler on Linux:

```sh
on1xc --emit-c consumer.on1x -o consumer.c
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Ion1x/runtime -Iinclude \
  consumer.c on1x/runtime/on1x_runtime.c \
  on1x/stdlib/http/runtime.c on1x/stdlib/http/kernels.c \
  my_host_policy.c -o consumer
```

`kernels.c` is the standalone implementation translation unit. Omit it if
the host already provides the full MuLIB dependency closure in a single
implementation unit; don't compile the same implementation switches twice.
No OpenSSL runtime library is linked. The standard CMake install copies the
package sources; no automatic native linkage or restricted project-bundle
support is supplied.

## MuLIB coverage

| Kernels | Role |
| --- | --- |
| `http`, `http1`, `webclient` | HTTP/1.1 framing and transactions use `http` via `webclient`. `http1` is the explicit C framing alias; raw messages, serializers, parsers and callback streaming are not exported. |
| `muw3`, `socket`, `uri` | Used for plain TCP transport and URL handling. Full MuW3 redirect/form/stream functions and raw sockets are not exported. |
| `https`, `tls`, `tsl`, `x509` | Used internally for verified native TLS. No TLS sessions, keys, trust loading, or callback interfaces cross the language boundary. |
| `http2` | C frame codec, not a complete HTTP/2 connection. Not used or exposed. |
| `http3`, `quic` | C framing, stream processing, and native HTTP/3 GET support in MuLIB. Neither live HTTP/3 nor raw stream/client APIs are exposed by this package. No HTTP/2 or HTTP/3 negotiation or fallback. |
| `http_request`, `http_response`, `httpserver_config`, `webserver`, `mcp-http` | Separate C request/response or service-side facilities. No listener, server, upgrade, or MCP support. |

The only raw extern in the On1x sources is `on1x_http_exchange_v1`. It borrows
an immutable NUL-free URL and accepts an integer GET/HEAD selector. The C
bridge serializes a response into one immutable runtime snapshot before On1x
decodes it, freeing native resources first. No pointer-valued handles,
callbacks, or "last result" globals are exposed. This ABI is not independently
reviewed ExoML/Chisel evidence; do not mark it admitted.

## Tests

Run `python3 -B on1x/stdlib/http/tests/run.py /path/to/on1xc cc`, or run
`ctest -R '^on1x\.stdlib\.http$'` from a native Linux On1x build. The loopback
fixture compiles actual On1x C11 output and uses ephemeral OpenSSL-generated
certificates and a Python TLS 1.3 peer solely as the test oracle. It never
contacts a network service off loopback.
