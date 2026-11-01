# Prowse control and the lprowse automation DSL

`include/mulib-prowse-control.h` combines [browser protocols](mulib-prowse-protocol.md),
[Mune Lua](mulib-mune.md), and [MuLIB Readline](mulib-readline.md). It provides C
automation functions, a persistent Lua environment with `require('lprowse')`, and
a syntax-highlighted, completing REPL. Source for the Lua library is in
[`library/lprowse/lprowse.lua`](../library/lprowse/lprowse.lua).

## Build and quick start

```sh
cc -std=c11 -Iinclude examples/modules/prowse-control.c -pthread -lm -ldl -o prowse
./prowse --repl
```

Start an appropriate driver externally, then use the REPL:

```lua
browser.connect('webdriver', 'http://127.0.0.1:4444')
browser.start({browserName='firefox'})
browser.go('https://example.com').wait('h1')
=browser.text('h1')
=browser.json(browser.links())
browser.quit()
:quit
```

For Chrome use chromedriver and `browserName='chrome'`, or connect to the
`webSocketDebuggerUrl` of a Chrome **page** from `/json/list` using protocol `cdp`.
For WebKitGTK start `WebKitWebDriver --port=4444` and use
`browserName='MiniBrowser'`; Safari uses safaridriver and `browserName='safari'`.
Driver/browser availability and setup follow those projects' platform requirements.
Firefox can also be started with `--marionette` and connected directly:

```sh
./prowse marionette marionette://127.0.0.1:2828 library/lprowse/examples/local-smoke.lua
./prowse webdriver http://127.0.0.1:4444 library/lprowse/examples/scrape.lua
```

The CLI form with protocol/endpoint starts an empty-capabilities session, executes
a script or REPL, and attempts session cleanup. `--repl` starts disconnected so
scripts can select their own capabilities. With no arguments the example runs an
offline self-check, suitable for automated example discovery.

## Implementation placement

Define all switches in one implementation TU before including the control header.
[`examples/prowse-common.h`](../examples/prowse-common.h) is the complete runnable
reference. Its switches are:

```text
MULIB_IMPL_PROWSE_CONTROL MULIB_IMPL_PROWSE_PROTOCOL MULIB_IMPL_READLINE
MULIB_IMPL_MUNE MULIB_IMPL_AST MULIB_IMPL_BASE64 MULIB_IMPL_BUFFER
MULIB_IMPL_DYNCALL MULIB_IMPL_DYNLOAD MULIB_IMPL_EGRAPH MULIB_IMPL_GIT
MULIB_IMPL_GLOB MULIB_IMPL_HTTP MULIB_IMPL_INET4 MULIB_IMPL_INET6
MULIB_IMPL_INTE6 MULIB_IMPL_JIT MULIB_IMPL_JSON MULIB_IMPL_MIR
MULIB_IMPL_PARSEC MULIB_IMPL_PARSER MULIB_IMPL_PEG MULIB_IMPL_REWRITE
MULIB_IMPL_SOCKET MULIB_IMPL_STRBUF MULIB_IMPL_TERMKEY MULIB_IMPL_TERMUTILS
MULIB_IMPL_URI MULIB_IMPL_UTF8 MULIB_IMPL_VMWEAVE
```

Use `#define` for each name. `include-mulib-lprowse.h` is checked-in embedded Lua
data and needs no switch. Python and an installed Lua interpreter are not runtime
dependencies. Link POSIX builds with threads, math and the dynamic-loader library
(`-ldl` where required). C++ declaration clients link a C11 implementation TU.

## C ownership and lifecycle

`create(protocol,options)` borrows an optional protocol. `connect` replaces an
inactive connection and owns the new client. `start` enables CDP Page/Runtime or
creates a driver session; capabilities are a plain object, wrapped as WebDriver
`capabilities.alwaysMatch` or sent directly to Marionette. CDP capabilities are
ignored. `attach` instead adopts an existing session; a NULL CDP session selects
the already connected page. One controller has one active session.

`quit` deletes WebDriver/Marionette sessions; CDP disables Page/Runtime and leaves
the browser/page open. `disconnect` forgets local session state and closes an
owned transport without a remote request, allowing recovery after a broken stream.
A borrowed protocol remains caller-owned. `destroy` releases Lua/editor/owned
transport resources; call `quit` explicitly when remote cleanup is wanted.

All calls require external serialization; callbacks must not reenter/destroy
their controller. `lua` and `editor` return borrowed, lazily initialized objects.
The controller must outlive borrowed objects and every outstanding Mune result.
Destroy Mune results before destroying the controller.

Operations returning `mulib_json_t **` require `*value == NULL`, transfer ownership
only on success, and use `mulib_json_destroy` for release. Parameters remain
borrowed for each call. Errors use the protocol status enum; `control_error`
provides the latest browser diagnostic, while return codes identify local
argument/allocation failures.

| C operation | Behavior |
| --- | --- |
| `navigate` | Navigate; CDP acknowledges before document load, drivers follow their page-load strategy |
| `evaluate` | Evaluate one JS expression by value, rejecting CDP exceptions/non-JSON remote values |
| `query` | CSS selector; `text`, `html`, `href` or `attribute`; one result/null or bounded result array |
| `click` | Driver element interaction, or CDP scroll/focus plus mouse press/release |
| `type` | Driver send-keys or CDP focused text insertion; existing text is retained |
| `screenshot` | Return base64 PNG text |
| `wait` | Poll for a selector until timeout, including a bounded remaining request timeout |

Selectors/strings are JSON-quoted before insertion into JavaScript. Query limits
truncate the all-matches result to `query_items`. `wait` tests presence, not
visibility or layout stability. Selector/JavaScript/browser failures are returned
immediately. CDP clicks use viewport coordinates; advanced interaction, frames,
downloads, cookies, tabs and browser-specific commands use the raw protocol APIs.

## Lua API

`lprowse` and `browser` refer to the module installed in `package.loaded.lprowse`.
Use **dot calls**, e.g. `browser.go(url)`; chainable operations return that module.

* Lifecycle: `connect('cdp'|'webdriver'|'marionette',endpoint)`, `start(caps)`,
  `attach(id)`, `quit()`, `disconnect()`, `status()`.
* Automation: `go(url)`/`navigate(url)`, `eval(expression)`, `click(selector)`,
  `type(selector,text)`, `fill(selector,text)`, `wait(selector,ms)`, `sleep(ms)`.
  `fill` assigns the DOM value and dispatches input/change events; custom widget
  behavior can be implemented with `eval`. `back`, `forward`, and `reload` use
  JavaScript history/location and should be followed by an appropriate wait.
* Extraction: `text(selector,all)`, `html(selector,all)`,
  `attr(selector,name,all)`, `links(selector)`, `title()`, `url()`, `screenshot()`.
  `html` defaults to `html`, and `links` defaults to `a[href]` and returns resolved
  DOM `href` properties. Screenshot data is base64 text, not an implicit file write.
* Low level: `command(method,params)`, `http(method,path,body)`, `event()`,
  `resolve(relative,base)`. The first two select CDP/Marionette and HTTP respectively.
  They retain protocol-specific result shapes. `event()` returns `null` when empty.
* Data: `json(value)`, `array(table)`, `null`, `scrape(fields)`.
* Workflows: `retry(attempts,delay_ms,fn)`, `crawl(seed,options)`.

JSON objects become tables, arrays become tagged dense tables, and JSON null uses
the distinct `lprowse.null` singleton. `array()` preserves an empty JSON array.
`json` sorts object keys and rejects cycles, mixed/sparse arrays, non-string object
keys, nonfinite numbers and nesting beyond 32. Numbers follow JSON/Mune numeric
precision, not a lossless arbitrary-integer format. Browser errors raise ordinary
Lua errors containing the operation, diagnostic and status; handle with `pcall`.

```lua
local p = require('lprowse')
p.go('https://example.com').wait('h1', 5000)
local row = p.scrape {
  heading = 'h1',
  links = {selector='a', attribute='href', all=true},
  current = function(browser) return browser.url() end
}
print(p.json(row))
```

`scrape` accepts CSS strings, descriptor tables (`selector`, `attribute`, `html`,
`all`) or functions. `retry` calls `fn(browser,attempt)`, returns the first success,
and rethrows the final failure; use it where repeating the operation is suitable.

`crawl` runs breadth-first with URL-fragment removal and deduplication. Defaults
are 100 visited pages, depth 3 and 1000 queued URLs. Seeds must be absolute HTTP(S).
Options: `max_pages`, `max_depth`, `max_queue`, `same_origin` (default true),
`links` (CSS selector), `wait`, `timeout`, `delay`, `fields`,
`allow(url,depth)`, `visit(row,page,browser)`, `continue_on_error`, and `collect`.
The page object carries URL/depth. `collect=false` streams through `visit` without
retaining rows. The queue remains bounded; pages/depth cap at 100000/1000.
Results are `(rows,{visited,queued,remaining})`; failures stop traversal unless
`continue_on_error`, which collects an error row. URLs use a textual origin check,
and redirects follow the browser; this is a traversal filter, not a network policy.

## REPL and extension points

The persistent REPL evaluates one line at a time. `=expression` prints returned
values; `:begin` collects multiline Lua and `:end` executes it. `:load path.lua`
executes a bounded script. `:profile path.mlsyn|mlac|mldir` transactionally loads
editor definitions; `:help` displays usage and `:quit` exits. Ctrl-C discards the
current line/block, Ctrl-D on an empty line exits. Lua syntax/runtime failures
print diagnostics and leave the REPL usable. Variables declared local to a chunk
do not persist into later chunks; globals/closures do.

Readline provides Unicode-scalar editing, history, completion, coloring and
bracketed paste. Pipe input is accepted without terminal escape output. Lua's
`print`/`io` use Mune's normal standard streams; REPL prompts, errors and returned
values go to the supplied output descriptor. Arbitrary Lua strings are output as
data and may contain terminal control sequences.

Applications may bind extra native functions through the borrowed Mune state,
add editor rules/completions/directives, or use the editor independently. Expansion
directives produce Lua source by replacing `$*` with raw argument text. Action
directives require a host handler; the built-in REPL handles only its documented
actions. It reports unhandled actions rather than executing a shell command.

## Resource limits and verification

Zero fields select 1 MiB script/argument JSON, 1000 extracted nodes, 30-second waits
and 50-ms polling, plus Mune's default heap/step/call/result limits. Explicit
upper bounds are 1 MiB source, 1 million query nodes and one-hour waits. Protocol
wire limits apply independently. Lua is an extensible runtime with its standard
libraries; these quotas bound execution/storage, not application capabilities.

Run `python3 tools/check_prowse.py` for focused GCC-compatible compilation,
examples, offline/loopback protocol checks, all 45 profiles and pipe/PTY editing.
Use `CC=clang CXX=clang++ ... --sanitizers` for ASan/UBSan, and `--live-firefox`
for the optional isolated headless-browser smoke run. Full-repository checks may
also diagnose unrelated legacy-header inventory problems.
