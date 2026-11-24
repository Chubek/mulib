# JSON-RPC 2.0 over HTTP

`mulib-jsonrpc.h` constructs and validates JSON-RPC 2.0 requests, batches,
notifications, and replies. It sends one synchronous HTTP POST via the
[REST HTTP helper](mulib-rest.md). No method execution, automatic retries
or non-HTTP transports are included. Redirect behavior comes from MuW3.

## Build

Define `MULIB_IMPL_CDD_JSONRPC` in one C11 translation unit and link the
REST implementation (`MULIB_IMPL_CDD_REST`) and its full dependency closure.
The only direct repository include is `mulib-rest.h`; see the
[REST build instructions](mulib-rest.md#build) for private headers and
switches. C++17 consumers include the declaration headers without switches.
`MULIB_IMPL_JSONRPC` and the original `mulib_jsonrpc_*` symbols remain
available for compatibility.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels examples/modules/jsonrpc.c -pthread -lm -ldl -o jsonrpc
./jsonrpc
```

The [example](../examples/modules/jsonrpc.c) uses an offline MuW3 transport.

## Requests and replies

- `mulib_cdd_jsonrpc_make` builds an owned JSON object; release it with
  `mulib_json_destroy`. Method names must be nonempty strings and cannot
  start with the reserved `rpc.` prefix. Optional `params` must be an
  object or array. A NULL `id` pointer omits the member and sends a
  notification. A non-NULL ID can be a string, number or JSON null; prefer
  strings (null has special error semantics in JSON-RPC).
- `mulib_cdd_jsonrpc_exchange` validates a caller-supplied request or a
  batch of 1–1024 requests before using the transport. A single request
  with an ID requires a response object with that same ID. A batch must
  get exactly one response per non-notification request; response order
  is irrelevant. Duplicate request or response IDs, unrequested IDs and
  missing replies are invalid. Request IDs and all user data are borrowed
  and must remain unchanged during the call.
- `mulib_cdd_jsonrpc_call` builds and exchanges a single request.

An exchange containing only notifications requires an empty HTTP response
body, commonly HTTP 204. Calls expecting a response never accept an empty
body. Nonempty responses must use version string `"2.0"` and contain exactly
one of `result` (which may itself be JSON null) or `error`. An error must
have an integer `code` and string `message`, and may contain `data`.
Strings are correlated by type and all decoded bytes; numeric IDs compare
as the MuLIB JSON DOM's doubles, so large IDs can lose precision. Use
strings when exact identity is required. `remote_error` is set when any
validated response is an error object. As in the [JSON kernel](mulib-json.md),
duplicate member names within one JSON object use the last value; this is
distinct from duplicate IDs across batch items, which are rejected.

Return `1` means a valid completed exchange; check the HTTP status and
`remote_error` independently. Invalid request/response structure and
`call` construction failure return `-1`; exceeding the batch limit
returns `MULIB_MUW3_LIMIT`, and failure to allocate the correlation table
returns `MULIB_MUW3_NOMEM`. Other REST/MuW3 errors are propagated. Failure
leaves the output unchanged. Initialize the reply to zero and use
`mulib_cdd_jsonrpc_reply_free` to release/reset it before reuse.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-jsonrpc.h`](../kernels/mulib-jsonrpc.h) |
| Implementation switch | `MULIB_IMPL_CDD_JSONRPC` |
| Repository-local includes | [`kernels/detail/mulib-jsonrpc-envelope.h`](../kernels/detail/mulib-jsonrpc-envelope.h), [mulib-rest](mulib-rest.md) |
| Complete example | [`examples/modules/jsonrpc.c`](../examples/modules/jsonrpc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/jsonrpc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
