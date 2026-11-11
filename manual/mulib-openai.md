# OpenAI protocol utilities

**Header:** `include/mulib-openai.h` · **Switch:** `MULIB_IMPL_OPENAI`

Shared protocol support for the [client](mulib-openai-client.md) and
[server](mulib-openai-server.md). Uses MuLIB HTTP, JSON, UTF8 and their dependency
closures. No third-party runtime libraries are used.

## Specification coverage

The bundled [OpenAI OpenAPI publication](../standards/openai-openapi/README.md),
API version 2.3.0, describes **388 REST operations**. Every operation is available
by its exact `operationId`, including capitalization, punctuation and upstream
spellings such as `submitToolOuputsToRun`. The catalog includes Responses (and
fixed-query beta variants), Chat Completions, Completions, models, embeddings,
moderation, audio, images, video, files/uploads, vector stores, conversations,
Assistants/threads/runs, batches, fine tuning, evaluations, realtime/live call
setup, organization/project administration, agents, skills, vaults and webhooks.

`operation_count`, `operation_at`, and `operation` expose immutable static
metadata: method, path template, accepted request content types, required-body
flag. Payloads use MuLIB JSON values or raw bytes so all published fields,
multimodal content, tool definitions and extension fields remain representable.
The library does **not** generate C structs for the 2,260 schemas or perform full
JSON Schema validation. Applications implement endpoint semantics, required
properties, permissions, model inference, resource storage and event sequences.
Realtime WebSocket/WebRTC media transport is separate from the REST call/session
setup operations covered here; it is not implemented by these headers.

The generated catalog lives in `include/detail/mulib-openai-operations.inc` and
records the source SHA-256. Regenerate/check it without dependencies:

```sh
python3 tools/generate_openai.py
python3 tools/generate_openai.py --check
```

The upstream generated specifications are inputs and are never rewritten by this
tool. Operation metadata is derived from the MIT-licensed OpenAI publication.

## Routing and encoding

`target` expands path parameters into individual percent-encoded segments and
appends encoded query pairs. Parameters are `mulib_http_header_t` name/value
pairs borrowed for the call. Query names can repeat (for example `include[]`).
Missing, duplicate or extra path parameters are rejected. Dots are encoded to
prevent a parameter from introducing literal `.`/`..` segments. Fixed catalog
query predicates cannot be overridden by added query fields. Zero target limit
selects 8192 bytes. Successful target strings are malloc-owned.

`match` operates on targets **relative to the API base**, such as
`/responses/resp_1?include[]=message.output_text.logprobs`. Static paths win over
parameter paths and fixed query predicates win over their ordinary variants.
Passing NULL as the method tests whether any method matches that path.

`path_parameter` and `query` return decoded malloc-owned strings; `query` returns
the first value for repeated names. Malformed escapes, NUL/control bytes and
invalid UTF-8 are rejected. Outputs remain unchanged on failure. Routing itself
does not interpret application parameter types; handlers should use the decoding
functions and validate their IDs and query values.

## JSON helpers and results

* `clone` creates an independent owned value.
* `error` creates `{ "error": { "message", "type", "param", "code" } }`.
  NULL `param`/`code` become JSON null.
* `output_text` concatenates Responses `output[].content[]` blocks of type
  `output_text`, or extracts the first Chat/Completion choice. It returns an owned
  JSON string, retaining embedded NULs. Read it using `mulib_json_text` and its
  explicit length. NULL means no text or allocation failure.
* JSON values are freed with `mulib_json_destroy`; C strings with `free`.

Results: `OK=1`, `INVALID=-1`, `LIMIT=-2`, `NOMEM=-3`, `TRANSPORT=-4`,
`CANCELLED=-7`, `API_ERROR=-8`, `PROTOCOL=-9`, `NOT_FOUND=-10`.
`strerror` returns borrowed static text. An HTTP API error is distinct from
transport failure; the client retains the error response for inspection.

See [the standalone catalog example](../examples/modules/openai.c) for the full
implementation-switch closure. `make check-openai` verifies every operation's
construction/routing and the dependent client/server workflows.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-openai.h`](../include/mulib-openai.h) |
| Implementation switch | `MULIB_IMPL_OPENAI` |
| Repository-local includes | [`include/detail/mulib-openai-operations.inc`](../include/detail/mulib-openai-operations.inc), [mulib-http](mulib-http.md), [mulib-json](mulib-json.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/openai.c`](../examples/modules/openai.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/openai.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
