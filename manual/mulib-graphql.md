# GraphQL over HTTP

`mulib-graphql.h` sends a synchronous GraphQL JSON POST using the
[REST HTTP helper](mulib-rest.md). It does not parse or execute the GraphQL
query language, validate a schema, perform subscriptions, or handle multipart
incremental responses.

## Build

Define `MULIB_IMPL_CDD_GRAPHQL` in one C11 translation unit and link the
REST implementation (`MULIB_IMPL_CDD_REST`) and its complete dependency
closure. The only direct repository include is `mulib-rest.h`; see the
[REST build instructions](mulib-rest.md#build) for private headers and
switches. Headers may be included without implementation switches from
C++17. `MULIB_IMPL_GRAPHQL` and the original `mulib_graphql_*` symbols remain
available for compatibility.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels examples/modules/graphql.c -pthread -lm -ldl -o graphql
./graphql
```

The [example](../examples/modules/graphql.c) uses an offline MuW3 transport.

## Exchange

`mulib_cdd_graphql_execute` requires a nonempty query string. Optional
`operation_name` must be a GraphQL Name (`[_A-Za-z][_0-9A-Za-z]*`); optional
`variables` must be a JSON object. Inputs are borrowed. Unless overridden by
the caller, `Content-Type` is `application/json`, and `Accept` prefers
`application/graphql-response+json` with `application/json` as a fallback.
The default `Accept` takes one of the REST helper's 254 caller-header slots;
MuW3 can impose a lower effective HTTP header limit.
There are no automatic retries; redirects follow MuW3 configuration.

A response must be a JSON object containing `data`, a nonempty `errors`
array, or both. `data` is an object; it can also be null if errors are present.
Optional response `extensions` must be an object. Every error contains a
string `message`; optional `locations`, `path`, and `extensions` are checked
for valid shape. This checks the envelope, not data against the query
selection set. The JSON envelope is checked irrespective of response
`Content-Type`, so older HTTP servers without a JSON media-type declaration
are usable. Shape validation alone does not establish that an intermediary's
HTTP error was produced by the GraphQL service.

Success (`1`) means a valid envelope; the reply may still be an HTTP error
(check `result.response.http.message.status`) or contain GraphQL errors
(`result.has_errors`). `-1` denotes invalid arguments or reply shape and
`-2` denotes local request construction failure. Other REST/MuW3 failures
are propagated and can share numeric values. Failure leaves the output
unchanged. Initialize the result to zero and call
`mulib_cdd_graphql_result_free` before reusing it; the result owns the HTTP
message and parsed JSON.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-graphql.h`](../kernels/mulib-graphql.h) |
| Implementation switch | `MULIB_IMPL_CDD_GRAPHQL` |
| Repository-local includes | [mulib-rest](mulib-rest.md) |
| Complete example | [`examples/modules/graphql.c`](../examples/modules/graphql.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/graphql.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
