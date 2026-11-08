# Static-file responses

[Manuals](README.md) · [Library index](index.md)

Include `mulib-static_file.h` and define `MULIB_IMPL_STATIC_FILE` in exactly one
translation unit to use the `mulib_static_file_reply` API.

## Dependencies

Define the dependency implementation switches separately: `MULIB_IMPL_VFS`,
`MULIB_IMPL_MIME`, `MULIB_IMPL_SECURITY`, `MULIB_IMPL_HTTP_RESPONSE`,
`MULIB_IMPL_WEBFRAMEWORK`, `MULIB_IMPL_HTTP`, `MULIB_IMPL_URI`,
`MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_PARSEC`, and `MULIB_IMPL_BUFFER`.
Headers never define another module's implementation switch.

## Contract

`mulib_static_file_reply(vfs, path, limit, out)` serves a regular file from a
borrowed virtual filesystem. Supply an absolute VFS path without a query.
The helper validates and percent-decodes the path once, checks the file's size
against `limit`, reads its complete contents, and creates an owned HTTP 200
response with a MIME type inferred from the path. Empty files are supported.
The filesystem must remain alive and stable for the operation.

Initialize `out` to zero. Release any previously owned response with
`mulib_http_message_free` before reusing the output for another successful call.
On failure, the previous output remains unchanged. Release each successful
response with `mulib_http_message_free` when finished.
Temporary storage and the response body require memory proportional to file size.

Return values are `MULIB_HTTP_OK`, `MULIB_HTTP_INVALID` for invalid paths,
missing/nonregular files or failed reads, `MULIB_HTTP_LIMIT` for oversized files,
and `MULIB_HTTP_NOMEM` for allocation failure. Callers choose the public HTTP
error status; lookup failures do not automatically produce a 404 response.

No query handling, directory index, ranges, native filesystem traversal or
symlink handling is provided. See [storage/server contracts](storage-server.md).

Run [`examples/modules/static_file.c`](../examples/modules/static_file.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-static_file.h`](../include/mulib-static_file.h) |
| Implementation switch | `MULIB_IMPL_STATIC_FILE` |
| Repository-local includes | [mulib-http_response](mulib-http_response.md), [mulib-mime](mulib-mime.md), [mulib-vfs](mulib-vfs.md) |
| Complete example | [`examples/modules/static_file.c`](../examples/modules/static_file.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/static_file.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
