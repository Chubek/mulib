# Prowse protocol: browser connections

`include/mulib-prowse-protocol.h` supplies synchronous browser-protocol clients,
including stream framing, native POSIX TCP transport, response correlation and
owned JSON replies. Use [Prowse control](mulib-prowse-control.md) for portable
browser operations and the `lprowse` Lua DSL.

## Protocol selection

| Kind | Endpoint | Browser integration |
| --- | --- | --- |
| `MULIB_PROWSE_CDP` | `ws://127.0.0.1:9222/devtools/page/<id>` | Chrome/Chromium DevTools page target |
| `MULIB_PROWSE_WEBDRIVER` | `http://127.0.0.1:4444` or `/wd/hub` base | geckodriver, chromedriver, safaridriver, WebKitWebDriver |
| `MULIB_PROWSE_MARIONETTE` | `marionette://127.0.0.1:2828` | Firefox started with `--marionette` |

Browsers/drivers are started by the application or user. CDP page target URLs
come from Chrome's `/json/list` HTTP endpoint; the browser-level URL needs target
attachment before page operations. Modern Firefox is supported through WebDriver
or Marionette, independently of its CDP compatibility. WebKit support uses its
W3C driver, rather than assuming Chrome's CDP works with WebKit Inspector.

Native transport accepts numeric IPv4, bracketed IPv6, and `localhost` (mapped
to `127.0.0.1`). It uses nonblocking sockets, close-on-exec descriptors, bounded
poll waits and partial I/O. A caller-supplied `mulib_prowse_io_t` supports other
platforms, DNS, TLS or test transports. `wss://` and `https://` require a TLS-capable
adapter; native transport returns `UNSUPPORTED`. Endpoint userinfo and fragments
are rejected. WebDriver base queries and Marionette paths/queries are rejected.

## Build and dependencies

Define `MULIB_IMPL_PROWSE_PROTOCOL` in exactly one C11 translation unit along with
`MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`,
`MULIB_IMPL_HTTP`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_URI`, `MULIB_IMPL_INTE6`,
`MULIB_IMPL_INET6`, `MULIB_IMPL_INET4`, `MULIB_IMPL_SOCKET`, `MULIB_IMPL_BASE64`,
and `MULIB_IMPL_GIT`. SHA-1 is used only for RFC 6455 handshake compatibility.
Enabling the optional parser-loader implementation additionally needs
`MULIB_IMPL_PARSER`, `MULIB_IMPL_AST`, `MULIB_IMPL_PEG`, and `MULIB_IMPL_GLOB`.
No implementation macros are enabled implicitly. C++ clients include declarations
and link the C implementation.

The standalone example shares the complete control-stack implementation unit:

```sh
cc -std=c11 -Iinclude examples/modules/prowse-protocol.c -pthread -lm -ldl -o prowse-protocol
./prowse-protocol                         # offline checks
./prowse-protocol http://127.0.0.1:4444   # GET /status
./prowse-protocol http://127.0.0.1:9222 /json/list
```

## Ownership and API

`connect(kind,endpoint,options,io,&error)` copies endpoint/options/callbacks and
returns an owned client. It opens/handshakes CDP and Marionette immediately;
WebDriver opens one connection per HTTP request. `io == NULL` selects native I/O.
An unsuccessful adapter `open` must clean up its own partial resources. Successful
opens are paired with `close`. Callback context remains application-owned.

Initialize every `mulib_prowse_reply_t` to zero. `call(client,method,params,&reply)`
accepts an optional borrowed JSON object and implements CDP/Marionette requests.
`http(client,method,path,body,&reply)` implements WebDriver GET/POST/DELETE; `path`
starts with `/` and is appended to the configured base. Parameters are copied
for serialization and remain caller-owned.

On `OK` or `REMOTE`, `reply.json` owns the complete envelope and `reply.value`
borrows its result/error. CDP exposes `result` or `error`; Marionette exposes the
fourth response-array member (or third error member); HTTP exposes W3C `value`,
falling back to the complete JSON body for discovery endpoints. Some Marionette
commands wrap their result in another `value`; the control layer normalizes this.
HTTP status and command ID are retained. `mulib_prowse_reply_free` destroys/reset
the reply. Other failures preserve the empty output. Do not copy owning replies.

`session(client,id)` copies a flattened CDP session ID; NULL selects the root
connection. `event(client,&json)` transfers the oldest queued CDP event and returns
`STATE` if none exists. Events are collected while waiting for command responses;
there is no background event reader. `dropped` counts oldest-event evictions,
saturating at `UINT64_MAX`. `timeout(client,ms)` changes the per-operation timeout
and returns its previous value; zero queries it.

All access is externally serialized. Adapter callbacks must not reenter or
destroy the client. `destroy` releases queued events and transport resources,
but sends no browser-session deletion command.

## Wire behavior and failures

* CDP uses RFC 6455 version 13: a random handshake nonce, verified accept hash and
  upgrade headers, freshly masked client frames, strict unmasked server frames,
  text fragmentation/reassembly, UTF-8 validation, ping/pong and close replies.
  Binary messages, compression/extensions, subprotocol negotiation and reserved
  opcodes are rejected. Close status and reason are validated.
* Request IDs increase from 1 through `INT32_MAX` without reuse. CDP responses
  must match both ID and flattened session; result/error envelopes and event
  shapes are checked. Marionette supports protocol version 3, its greeting and
  decimal-length-prefixed `[0,id,method,params]` / `[1,id,error,result]` messages.
* WebDriver uses the [HTTP module](mulib-http.md): content-length, plain chunked,
  or close-delimited responses. Nonempty trailers, chunk extensions, ambiguous
  framing, content decompression and automatic redirects are outside that subset.
  Non-2xx JSON responses yield `REMOTE`; a non-JSON response yields `FORMAT`.
  Successful result objects may contain ordinary data named `error` without being
  mistaken for protocol failures. Remote JavaScript exceptions may still live inside
  CDP results; the control layer checks them.
* A stream failure after sending begins, malformed response, timeout, mismatched
  ID/session, or exhausted receive-work budget closes CDP/Marionette. Reconnect
  before another command. Remote command errors leave the stream usable. HTTP
  requests close their individual connection on all outcomes.
* `INVALID`, `MEMORY`, `LIMIT`, `IO`, `TIMEOUT`, `CLOSED`, `FORMAT`, `REMOTE`,
  `UNSUPPORTED`, `STATE` and `BUSY` are distinct. Recognized JSON allocation
  diagnostics map to `MEMORY`; ambiguous parser failures map to `FORMAT`.
  `mulib_prowse_strerror` returns stable explanatory text.

## Limits

Zero option fields select 8 MiB per message, 64 queued events, 1024 received
messages/frames per receive loop, and a 10-second request timeout. The deadline
covers opening, writing and receiving, using a monotonic clock where available
and C11 wall time otherwise. Adapters must honor the remaining timeout provided
on every callback. Queue memory can reach `queued_events * message_bytes` plus
JSON/storage overhead; these are logical wire limits, not exact malloc quotas.

Outgoing trees are preflighted to depth 64 and a node/key/string-byte work budget
before copying. Serialized bytes are checked separately. Incoming JSON inherits
the JSON module's depth/work limits. Host/session strings are bounded to 255 bytes,
the endpoint path to 2047 bytes, and HTTP request targets to 4095 bytes. These
bounds fail explicitly. No automatic request retry hides uncertain remote effects.

## Verification

`tests/test_prowse.c` covers short I/O, framing, masked client messages, fragmented
server text with interleaved pings, request correlation, remote errors, timeouts,
session URL escaping, control operations and Lua workflows.
`tools/check_prowse_io.py` exercises actual loopback sockets and REPL descriptors.
`tools/check_prowse_live.py` runs the local smoke workflow in an isolated headless
Firefox profile; it requires Firefox and does not use the user's normal profile.

Protocol references: [CDP](https://chromedevtools.github.io/devtools-protocol/),
[W3C WebDriver](https://www.w3.org/TR/webdriver2/),
[Marionette](https://firefox-source-docs.mozilla.org/testing/marionette/Protocol.html),
[RFC 6455](https://www.rfc-editor.org/rfc/rfc6455).
The implementation is original MuLIB code.
