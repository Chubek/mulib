# MIME lookup and Content-Type normalization

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Case-insensitive filename-extension lookup and bounded HTTP Content-Type normalization. No content sniffing, encoding detection or allocation.

## Integration

Define `MULIB_IMPL_MIME` and `MULIB_IMPL_SECURITY` in exactly one C11 translation unit. Include `mulib-mime.h` normally elsewhere. See [storage/server contracts](storage-server.md).

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-mime.c -o /tmp/test-mime
/tmp/test-mime
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/mime.c -o /tmp/example-mime
```

## API

```c
const char *mulib_mime_type(const char *filename);
int mulib_mime_content_type(const char *header,char *out,size_t cap);
```

## Safety and ownership

`mulib_mime_type(filename)` retains the existing filename API. It returns a static immutable MIME string; never free it. NULL, unknown/missing extensions and directory-only extensions return `application/octet-stream`. Both slash styles reset extension tracking. The table covers html/htm, css, js, json, txt, svg, png, jpg/jpeg, gif, ico, pdf, wasm and xml. HTML/CSS/JS/plain text include `charset=utf-8` without validating file bytes. Pass a filename, not a URL with query/fragment.

`mulib_mime_content_type` copies lowercase HTTP `token/token`, excluding parameters. Both components must be nonempty, with exactly one slash and no embedded whitespace. Optional leading/trailing horizontal whitespace is accepted; trailing junk and controls are rejected. Parameters after `;` are not parsed (use [charset](mulib-charset.md) for charset extraction). Returns 1 success, 0 invalid/too small with output unchanged. Capacity includes NUL; input/output must not overlap.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-mime.h`](../include/mulib-mime.h) |
| Implementation switch | `MULIB_IMPL_MIME` |
| Repository-local includes | [mulib-seurity](mulib-seurity.md) |
| Complete example | [`examples/modules/mime.c`](../examples/modules/mime.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mime.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
