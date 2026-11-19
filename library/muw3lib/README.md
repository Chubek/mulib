# muw3lib — Mune/MuW3 HTTP clients

`muw3lib` provides synchronous HTTP protocol adapters and selected operations
for ten service APIs. It runs inside the [MuW3 Weblet host](../lmuw3/README.md)
with no additional Lua, curl, or browser dependency.

## Run

From the repository root:

```sh
make muw3
build/muw3 --weblet library/muw3lib/examples/demo.lua
```

The demo runs offline unless given a URL. Add the package's parent directory
to `package.path` before importing it. From the repository root:

```lua
package.path = 'library/?.lua;library/?/init.lua;' .. package.path
local api = require('muw3lib')
local r = api.fetch('https://example.org/', {headers={Accept='text/html'}})
print(r.status, #r.body)
```

CMake installs the package under `<prefix>/share/mulib/muw3lib`. Use
`<prefix>/share/mulib/?.lua` and `<prefix>/share/mulib/?/init.lua` in
`package.path` for an installed copy. The host embeds `lmuw3` but not
`muw3lib`. Use `lmuw3` 1.1 or a compatible host exposing
`request_redirect_control`; older hosts cannot enforce the adapter's
redirect policy. Standalone Lua can run the mock suites, not native HTTP.

## Requests

`api.rest.new(base_url, options)` constructs a reusable client. Its
`:request(method, path, options)` returns the original MuW3 response;
`:json(...)`, `:get(path, query)`, and `:post(path, payload)` return decoded
data and the original response (`nil, response` when the body is empty).
It also provides `put`, `patch`, `delete`, `head`, and `options` helpers.
Requests accept at most one of `body`, `json`, or `form`. Options include
per-request headers and query fields; use ordered name/value pairs to
preserve repeat values. Named header overrides are case-insensitive.
`max_body_bytes` sets an application limit on the buffered body; a
per-request value overrides the client's setting, and the host's limits
still apply while receiving it. Transport,
parsing and validation failures raise Lua errors; use `pcall` for recovery.
Non-2xx status is an error for the client unless `allow_error` is set.
In contrast `api.fetch` and
`api.browser.fetch` are the same function and return HTTP error statuses
as ordinary responses; neither returns a Promise or a stream.

`api.core.muw3` exposes the underlying host. The host uses `null` for JSON
null and `array({})` for an empty array. Read its
[request, response and JSON reference](../../manual/mulib-muw3-mune.md) for
details and document/transport limits.

## Coverage

| Module | Selected operations |
|---|---|
| `github` | Repositories, issues, pull requests, contents, commits, repository search |
| `openai` | Responses, embeddings, chat completions, models |
| `googlemaps` | Geocoding, legacy Directions, Places New text search |
| `stripe` | Customers, payment intents, checkout sessions, subscriptions |
| `slack` | Method calls, auth test, chat messages, conversation history |
| `twilio` | Message listing, SMS, outbound calls |
| `gitlab` | Projects, issues, merge requests, pipelines |
| `opencode` | Selected OpenCode V2 operations from the pinned OpenAPI contract |
| `wikipedia` | MediaWiki search, extracts, page summary |
| `openweather` | Current weather, five-day forecast, coordinate lookup |
| `graphql` | GraphQL-over-HTTP JSON POST; no subscription transport |
| `jsonrpc` | JSON-RPC 2.0 calls, notifications and correlated batches over HTTP |
| `soap` | SOAP 1.1 and 1.2 XML envelopes and HTTP transport, not an XML parser |
| `sse` | Parse/fetch finite event streams; no live callbacks or reconnection |

Service methods use dot syntax, e.g. `client.repository('owner', 'repo')`.
Provider contracts are not complete SDKs. See [CAPABILITIES.md](CAPABILITIES.md)
for scope; provider credentials, quotas, server versions and account
features can affect availability.

### Protocol examples

```lua
local rpc = api.jsonrpc.new('https://example.org/rpc')
local replies = rpc.batch({
  {method='report.read', params={name='today'}},
  {method='report.status'},
})
```

Batch replies are returned in request order, excluding notifications.
Remote error envelopes are retained in the batch. Single JSON-RPC
calls and GraphQL `execute` raise on remote errors by default; pass
`{allow_errors=true}` as their final options argument to inspect the
validated envelope. SOAP defaults to version 1.1; pass `{version='1.2'}`
to its constructor to use 1.2. SOAP request fragments must be trusted
XML; the adapter does not validate XML replies. Pass `{allow_error=true}` to
`soap.call` if you need the raw response from an endpoint that sends faults
with a non-2xx HTTP status.

`api.sse.parse(body, options)` returns events and final parser state;
`api.sse.fetch(url, headers, options)` returns events, the HTTP response,
and parser state. The default limits are 1 MiB of buffered text and
4,096 events, configurable with `max_body_bytes` and `max_events`.
Events require a terminating blank line. The parser rejects invalid UTF-8
rather than replacing it, and does not use the reported retry delay to
open another connection.

### Credentials and side effects

Use trusted origins and verified HTTPS for credentials. Do not accept
untrusted package search paths or server origins. The HTTP adapters
disable redirects before transport, including same-origin redirects; this
protects opaque API-key headers, query values, and bodies as well as
standard credentials. Redirect policy for callers that explicitly use
`lmuw3` directly is separate. The library does not sandbox a caller or
authorize a remote service. Explicit write operations can have remote
side effects; no automatic retry or pagination is performed.

OpenCode routes are based on the repository's
[pinned V2 schema](../../standards/opencode-v2/openapi.json).
The wrapper defaults to `http://127.0.0.1:4096`, accepts cleartext only
for loopback IP literals, has no `.raw` escape hatch, and does not discover
or start the server. Session writes are off unless the caller passes
`allow_session_writes=true`. The prompt helper supports text with queue
delivery and `resume=false`, not shell, command, or PTY execution.

gRPC and WebSocket are explicitly unsupported in this host. The C codecs
are not equivalent to Mune bindings or an HTTP/2/duplex transport. DOM,
browser storage, workers, WebRTC, browser crypto, geolocation, graphics and
other browser APIs are not emulated.

## Verify

```sh
make check-muw3lib
ctest --test-dir <cmake-build-dir> -R muw3lib --output-on-failure
```

CTest needs `MULIB_BUILD_MUW3=ON`; the real loopback test also uses Python 3.
The offline mock suites can be run under Lua 5.4+ (e.g.
`lua5.4 library/muw3lib/tests/mock.lua`). The tests do not contact
third-party APIs or require real credentials.
