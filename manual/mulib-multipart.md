# mulib-multipart: Multipart/form-data boundary and field formatting for simple text fields.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Multipart/form-data boundary and field formatting for simple text fields. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_MULTIPART` in exactly one C11 translation unit. Use `#include "mulib-multipart.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-multipart.c -o /tmp/test-multipart
/tmp/test-multipart
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/multipart.c -o /tmp/example-multipart
```

## API

```c
/* Format single text field into caller buffer; returns byte length or 0 on error. */
size_t mulib_multipart_field(const char *boundary,const char *name,const char *value,char *out,size_t cap);
```

## Safety and ownership

Formats one text field ending with CRLF, not the final `--boundary--` delimiter. Boundary must contain 1..70 ASCII alphanumeric, hyphen or underscore bytes. Name must be nonempty without controls, quotes or backslashes. Values are NUL-terminated strings, not binary payloads; boundary delimiter line prefixes in values are rejected. The caller supplies a suitable boundary and appends the closing delimiter. Returns byte count excluding NUL, or 0 on invalid/capacity/formatting error without writing output. Capacity includes NUL; no overlap. This does not encode filenames or files.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-multipart.h`](../include/mulib-multipart.h) |
| Implementation switch | `MULIB_IMPL_MULTIPART` |
| Repository-local includes | None |
| Complete example | [`examples/modules/multipart.c`](../examples/modules/multipart.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/multipart.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
