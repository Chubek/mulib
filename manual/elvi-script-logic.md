# ElviScript logic: Mune and XPath

The ElviScript scrape coordinator supplies a successful HTML response to the
logic stage. `kernels/mulib-elvi-script-logic.h` parses the response in memory
with MuLIB HTML/XML and evaluates selectors with `mulib_xpath_query`. Mune runs
the Lua source that the host explicitly supplied for the profile. The logic
stage never loads a script by filename, opens a URL, or writes an artifact.

## Lua lifecycle and capabilities

The source runs once to define global hooks. The stage captures those hooks at
the end of initialization; changing the global names later does not replace
the captured functions.

| Hook | When called | Native operations allowed |
| --- | --- | --- |
| `on_start()` (optional) | After initialization, before the first response | `enqueue` |
| `on_response(document, url)` (required) | For each accepted response, in FIFO order | All five operations below |
| `on_finish()` (optional) | Once, after the host has drained the queue | None |

Hook return values are ignored. `destroy` releases resources; it does not call
`on_finish`. An invalid hook, parse error, budget breach, invalid argument, or
rejected host callback fails the stage. There is no retry or replay.

* `xpath(node, expression)` returns a one-based, document-order sequence of
  opaque node handles. A valid expression with no matches returns an empty
  sequence. Absolute paths start from the document; other paths start from
  `node`. An initial `//` or `.//` is allowed; ancestor steps (`..`) and any
  later descendant axis are not. The supported tests and predicates are those
  in [MuLIB XPath](mulib-xpath.md); attributes are read separately.
* `text(node)` returns descendant text, excluding comments. `<br>` contributes
  a space; leading and trailing ASCII whitespace is removed, and interior
  ASCII whitespace collapses to one space. Non-ASCII spaces, such as `&nbsp;`,
  are preserved.
* `attr(node, name)` returns an attribute value or `nil`. Parsed HTML tag and
  attribute names are lowercase.
* `enqueue(url)` proposes a typed GET request. This is not a fetch operation.
  The coordinator seeds the first request and validates later requests against
  its exact-host policy and queue bounds.
* `emit(name, url [, price [, location]])` proposes a typed record. Name and
  URL must be nonempty strings; price and location may be nil or omitted.
  Price is display text. Strings must fit the typed record fields and be valid
  UTF-8 with no embedded NUL. URLs are absolute ASCII HTTPS URLs and the scrape
  coordinator applies the same exact-host policy to result URLs.

Documents and selected nodes are userdata, not addresses, tables, or
numbers. A handle is valid only within the response that issued it. Retaining
one in a global or closure does not retain the DOM; passing it back to a
native operation on a later response fails. Strings explicitly copied into Lua
and ordinary Lua state can be retained, subject to the runtime heap budget.

## Input and resource limits

The logic API accepts successful HTTP 2xx messages with a bounded, nonempty
body and a valid effective URL. The scrape coordinator separately checks
response provenance and HTTP metadata. Input must be full explicit-tag UTF-8
HTML with an `html` root (an HTML doctype is optional). The parser decodes
supported named and numeric entities and handles void tags such as `<br>`.
This is not browser HTML: malformed markup, implicit end tags, raw-text
elements such as `script` and `style`, and foreign content are rejected. No
scripts, embedded resources, or network-dependent entities are resolved.

| Resource | Limit |
| --- | --- |
| Lua source | 16 KiB; 4,096 tokens, AST nodes and semantic records each; 1,000,000 frontend work steps |
| Mune execution | Tree mode only; 2 MiB runtime heap, 200,000 execution steps per invocation, 32 call frames, 256 values per list |
| HTML per response | 2 MiB, 1,024 lexical events, 512 DOM nodes, depth 32 (document at depth zero), 4 KiB per start tag |
| XPath per response | 256 bytes and 8 steps per expression, 64 matches per query, 512 issued handles total; cumulative traversal and ordering work budget |
| Native bindings | At most 1,024 calls per hook; cumulative 4 MiB text-inspection budget per response and at most 4 KiB per returned `text` value |
| Stage I/O | At most 8 enqueue proposals, 8 responses and 64 emitted records per stage; the coordinator may reject earlier |

`mulib_mune_state_create_bare` does not install any standard libraries, global
`_G`, primitive string metatable, module searchers or package paths. There is
no `io`, `os`, `debug`, `package`, `require`, `load`, protected call, coroutine,
time, random, environment, socket, process or dynamic native module API in this
stage. These are in-process capability restrictions and resource bounds, not
an OS sandbox or a wall-clock execution guarantee. Lua source and the linked
runtime must be trusted by the host. Callback time is not bounded by Lua's
step counter.

The C interface is synchronous, single-threaded and non-reentrant. Callback
arguments are borrowed only during the call. A callback result of zero
rejects the operation. A later error cannot undo earlier callbacks; direct
embedders must buffer requests or result proposals if they need transactional
publication. The scrape coordinator buffers records until the pipeline
finishes successfully. No response buffer or DOM escapes logic as a host
artifact.

## Offline fixture

[`BookingDotCom.lua`](../library/elvis/logic/BookingDotCom.lua) selects
`data-testid=property-card` elements and their title, title-link, price and
address descendants. It optionally enqueues a `pagination-next` link. Fixtures
use absolute same-host HTTPS links; the script does not resolve relative URLs.
The selectors are a synthetic fixture contract, not a claim of current live
site compatibility.

From the repository root, build and run the focused test with:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Ikernels \
  tests/test-elvi-script-logic.c -lm -ldl -pthread -o /tmp/test-elvi-script-logic
/tmp/test-elvi-script-logic
```

The test translation unit lists the required implementation switches for a
single-header embed. Define these switches in only one implementation
translation unit.
