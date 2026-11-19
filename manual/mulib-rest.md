# REST-oriented HTTP client

`mulib-rest.h` adds JSON request and response handling to the synchronous
[MuW3 HTTP client](mulib-muw3.md). REST is an architectural style; this module
does not interpret resource links, implement authentication or caching policies,
or select a pagination format.

## Build

Define `MULIB_IMPL_CDD_REST` in one C11 translation unit; other consumers
include the header without switches (C11 or C++17). `MULIB_IMPL_REST` and the
original `mulib_rest_*` symbols remain available for compatibility.

Direct repository includes are `mulib-muw3.h`, `mulib-json.h`, and
`mulib-uri.h` (implementation). Select `MULIB_IMPL_JSON`, `UTF8`, `MUW3`,
`HTTPS`, `TSL`, `WEBCLIENT`, `HTTP`, `URI`, `INET4`, `INET6`, `PARSEC`,
`BUFFER`, `STRBUF`, and `SOCKET`. The short switch names have the
`MULIB_IMPL_` prefix. Select the MuW3 crypto switches `TLS`, `ENTROPY`,
`SHA`, `HKDF`, `X25519`, `AES_GCM`, `DER`, `SIGNATURE`, `X509`, and `BASE64`
explicitly or with `detail/mulib-tls-implementation.h`. The SHA
implementation includes `detail/mulib-hash-core.h`; the URI implementation
uses `mulib-inte6.h` as the IPv6 compatibility include. Neither of these two
headers needs a separate implementation switch with the setup above.
`_POSIX_C_SOURCE=200809L` enables native DNS on POSIX systems.

Compile the offline standalone [example](../examples/modules/rest.c) from
the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/rest.c -pthread -lm -ldl -o rest
./rest
```

## Requests and ownership

`mulib_cdd_rest_request` takes a borrowed client, absolute HTTP(S) URL,
optional method (NULL means GET), up to 254 caller headers, and optional JSON
value (NULL means no body; a JSON null value is sent as `null`). Effective
HTTP header, URL and body limits can be lower; they are set by MuW3. JSON
encoding creates a temporary string before MuW3 applies its outbound body
limit. With a JSON body the client adds `Content-Type: application/json` by
default, and it adds `Accept: application/json` with or without a body;
caller fields of either name override the corresponding defaults regardless
of ASCII case. Use a verified HTTPS endpoint for sensitive data.

If `parse_json` is nonzero, the entire nonempty body must be one valid JSON
value, regardless of its media type or HTTP status. Empty bodies (e.g.
HTTP 204 or HEAD) leave `response.json` NULL. For non-JSON replies, set
`parse_json` to zero and use `response.http.message.body_length` to read
the raw body.

Return `1` means a completed transaction, including a valid 4xx/5xx reply;
inspect `response.http.message.status`. The local results are `-1` for invalid
inputs or unparseable requested JSON, `-2` for local allocation or JSON
encoding failure. MuW3 transport results pass through; numeric codes can
overlap. This wrapper does not retry; redirects follow the MuW3 settings.
Failure leaves `out` unchanged. Initialize it to zero and free before
overwriting another successful result. `mulib_cdd_rest_response_free` releases
both owned representations and resets the response; NULL and repeat calls are
safe.

## Query strings

`mulib_cdd_rest_url_query` validates a nonempty URI reference (absolute or
relative), preserving its spelling and any fragment. It appends fields
before the fragment, preserving existing query pairs and encoding new names
and values using form rules (e.g. space becomes `+`, literal `+` becomes
`%2B`). `max_bytes` applies to the whole result excluding its terminating
NUL, and defaults to 8192. The result is malloc-owned; NULL means invalid
input, exhausted limit or allocation failure. A URI reference accepted by
this helper need not be a valid MuW3 endpoint.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rest.h`](../include/mulib-rest.h) |
| Implementation switch | `MULIB_IMPL_CDD_REST` |
| Repository-local includes | [mulib-json](mulib-json.md), [mulib-muw3](mulib-muw3.md), [mulib-uri](mulib-uri.md) |
| Complete example | [`examples/modules/rest.c`](../examples/modules/rest.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rest.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
