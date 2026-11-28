# Browser network and Fetch integration status

The staging manifest describes separate bounded kernel profiles. These updates
do not register new kernels, enable live networking, or change the strict
outbound URL or origin admission rules.

| Area | Current slice | Still unsupported or not addressed here |
| --- | --- | --- |
| Upstream `fetch` / `browser-fetch` | The existing bounded callback adapter accepts final 200–599 responses, rejects nonempty 204/205/304 and unsafe Content-Type field bytes, and publishes atomically. The browser path inherits these checks. | No redirect handling, CORS, preflight, response filtering, streaming, credentials, or authenticated transport integration. `fetch-response` remains a separate staging helper, not a second response path. |
| `web-encoding` | One-shot UTF-8 replacement consumes the valid portion of an incomplete sequence as one error and reprocesses out-of-range bytes; existing Windows-1252 mapping retained. | No streaming, fatal mode, labels, BOM removal, UTF-16 or other legacy decoders. `web-textcodec` inherits replacement behavior but is not the JS TextDecoder API. |
| URI / URL / `urlcanon`, staging URL helpers | Unchanged: RFC 3986 references, component escaping, strict HTTP(S) keys, and staging decomposition remain distinct profiles. | No complete WHATWG URL state machine or IDNA/PSL-backed canonical site handling. |
| Transport | Existing HTTP/TLS/QUIC facilities are not wired into browser Fetch by this change. | Authenticated per-hop transport with resolved-peer/redirect checks and bounded timeout, receive and decompression handling remains a browser integration gate. No live tests. |
| Cookie and HTTP cache | Existing cookie-pair, browser jar and URL-keyed cache contracts are unchanged. | Host must authorize cookie Domain and partition credentials. SameSite/HttpOnly exposure, Cache-Control, Vary, and revalidation are not supplied by the browser jar/cache profile. |
| Other Network and Fetch staging rows | Unchanged. | Their bounded helpers do not constitute browser standards or security enforcement. |

`tests/test_browser_network.c` exercises a scripted in-process transport and
byte-only decoder vectors. Run it without network setup:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels \
  tests/test_browser_network.c -lm -o test-browser-network
./test-browser-network
```

Do not expose this path to untrusted content solely on these checks. The host
still needs trustworthy transport and origin/isolation enforcement; validation
of returned metadata cannot undo callback activity.
