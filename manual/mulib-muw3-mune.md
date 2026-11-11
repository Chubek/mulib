# MuW3 Mune engine and lmuw3

[Manuals](README.md) · [HTTP client](mulib-muw3.md) · [MicroChan](mulib-microchan.md)

**Header:** `include/mulib-muw3-mune.h` · **Switch:** `MULIB_IMPL_MUW3_MUNE`

The engine embeds Mune and the Lua module in `library/lmuw3/lmuw3.lua`. Each
engine has independent persistent globals, HTTP configuration and IPC connections.
Programs are called **Weblets**. `require('lmuw3')` and global `muw3` refer to the
same module without needing a Lua installation or a runtime module path.

## C embedding

Link CMake target `lmuw3`, or compile `library/lmuw3/runtime.c` together with
`library/microchan/runtime.c`, `-pthread -lm -ldl` on Linux. Those files enumerate
all implementation switches explicitly; a custom header-only TU needs the same
closure: MuW3/TLS/HTTPS/HTTP/socket, MicroChan/container-ipc/CRC32, JSON/YAML/UTF8,
and Mune/JIT/MIR/VMWEAVE/EGRAPH/REWRITE/DYNCALL/DYNLOAD/PARSER/PEG/AST/GLOB/PARSEC/STRBUF.
No dependency implementations are activated automatically by public headers.

```c
mulib_muw3_engine_t *engine = mulib_muw3_engine_create(NULL, NULL, NULL);
mulib_mune_result_t *result = NULL;
mulib_mune_error_t error;
const char code[] = "return require('lmuw3').json_decode('{\"answer\":42}').answer";
if (engine && !mulib_muw3_engine_run(engine, code, sizeof code - 1, &result, &error)) {
    /* result is GC-rooted until destroyed. */
    mulib_mune_result_destroy(result);
}
mulib_muw3_engine_destroy(engine);
```

Creation copies client/options; custom HTTP/TLS callback contexts remain borrowed
and must outlive the engine. `engine_lua` borrows the state for application bindings
or execution-mode selection. Free all Mune results before destroying the engine.
Execution is single-threaded/nonreentrant; destruction while executing is ignored.
Direct Mune calls through the borrowed state must also obey these lifetime rules.

Zero options select a 1 MiB source/data document limit, 16 channels, and Mune's
64 MiB heap/10 million steps/200 calls/100000 values limits. The document maximum
is 16 MiB, channel maximum 256; HTTP raw/body bounds come from the client instead.
Mune frontend token/node/source limits also apply to source execution. File loading
uses Mune's BOM/shebang-aware parser. Diagnostics follow Mune's borrowed lifetime
rules; engine-creation errors use static text. Runtime errors preserve preceding
Lua and external I/O side effects. Native I/O timeouts are independent of Lua steps.

## Requests and responses

```lua
local w = require('lmuw3')
local r = w.request {
  url = 'http://localhost:8000/items', method = 'POST',
  query = {page='1'}, headers = {Accept='application/json'},
  json = {name='example'}, bearer = 'token',
}
r:expect(201)
local value = r:json()
```

`request` requires a URL, defaults GET, and accepts `method`, `headers`, `query`,
`body`, `json`, `form`, `bearer`. Body, JSON and form are mutually exclusive. Body
is a binary Lua string. JSON/form select Content-Type unless supplied. Query/form
maps contain string keys/values; `{{name,value},...}` preserves duplicates/order.
Header input is copied logically, never modified. Native header/form lists are
bounded to 256 pairs (HTTP's configured header limit may be lower).

Response fields: `status`, `url`, `redirects`, binary `body`, ordered `headers`
and separate ordered `trailers`. Methods use colon syntax: `r:header(name)` returns
the first case-insensitive match; `r:json()`/`:yaml()` parse body; `r:expect(status)`
asserts and returns the response. 4xx/5xx are ordinary responses until asserted.
Transport/protocol/data errors raise Lua errors; use `pcall` to handle them.

`w.get(url,headers)` and `w.post(url,body,headers)` are conveniences.
`w.form(fields)` returns URL-encoded text; `w.resolve(base,reference)` uses RFC 3986.
`w.session(base,default_headers)` returns an object with **dot-call**
`session.request(spec)`: resolve URL against base, merge headers case-insensitively,
and execute. Defaults are application-controlled, including credentials; use
sessions for the intended service. `w.collection(specs,after)` runs an ordered
array of request specs, invoking optional `after(response,spec,index)` after each.
Collections can be decoded from JSON or MuLIB's supported YAML configuration subset.
Failure stops a collection; it does not undo completed requests.

## Structured data

`json_decode`, `json_encode`, `yaml_decode`, `yaml_encode` use MuLIB's data parsers
and serializers. `w.null` preserves JSON null; `w.array({})` marks empty arrays.
Decoded empty arrays retain their marker across round trips. Plain empty tables
encode as objects; nonempty dense integer-key tables encode as arrays. Cycles,
sparse arrays, mixed object keys, nonfinite numbers and invalid UTF-8 fail. Binary
HTTP/IPC strings can contain arbitrary bytes; JSON strings must be valid UTF-8.
JSON numbers use MuLIB's binary64 representation; this is not an exact arbitrary
precision integer serialization API. Conversion nesting is bounded to 64 levels.
YAML follows the subset documented in [mulib-yaml](mulib-yaml.md).

## IPC Weblets

```lua
local w = require('lmuw3')
local bus = w.channel('/absolute/private/bus.sock')
bus.subscribe('muw3/demo/events')
bus.publish('muw3/demo/events', 'hello\0world')
local event = assert(bus.receive(1000))
assert(event.body == 'hello\0world')
bus.close()
```

Channel operations use **dot calls**. Subscribe/unsubscribe/publish synchronously
wait for matching ACKs; nonzero daemon statuses raise errors. Events encountered
while waiting are retained in arrival order (up to 64 pending events). A command
ACK wait uses 30 seconds per receive; connection/send waits use the client timeout.
`receive(milliseconds)` returns an event or nil on timeout; omitted timeout is
30 seconds, zero polls, maximum one hour. Event fields are `opcode`, `id`, `status`,
`topic`, `body`. Lua correlation IDs are positive signed 64-bit values. Channel
handles have monotonically increasing engine IDs so closed handles cannot alias
a later connection. Close is idempotent in Lua; engine teardown closes all channels.

There is no implicit worker launch from library calls. The standalone runner
launches Weblets as normal processes; MicroChan transports messages between them.
See [worker](../library/lmuw3/examples/worker.lua) and
[client](../library/lmuw3/examples/client.lua) for a request/reply convention using
application-selected reply topics. Native C can also launch a Weblet with
MuLIB process/daemon tools or embed the same engine directly.

## CLI and verification

`muw3 --weblet FILE [ARGS...]` installs `arg[0]=FILE`, `arg[1..]` from subsequent
arguments and executes the file. Runtime diagnostics print filename/byte offset.
Ordinary Mune host libraries are available to the application.

`make check-muw3` verifies native HTTP/DNS, two communicating Weblets, profile
enforcement, binary strings, data conversion, persistent state and cleanup.
`python3 tools/embed_lmuw3.py --check` verifies that the embedded Lua matches source.
[`examples/modules/muw3-mune.c`](../examples/modules/muw3-mune.c) runs offline.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-muw3-mune.h`](../include/mulib-muw3-mune.h) |
| Implementation switch | `MULIB_IMPL_MUW3_MUNE` |
| Repository-local includes | [`include/detail/mulib-lmuw3.inc`](../include/detail/mulib-lmuw3.inc), [mulib-microchan](mulib-microchan.md), [mulib-mune](mulib-mune.md), [mulib-muw3](mulib-muw3.md), [mulib-yaml](mulib-yaml.md) |
| Complete example | [`examples/modules/muw3-mune.c`](../examples/modules/muw3-mune.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/muw3-mune.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
