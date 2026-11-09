# mulib-webextract: Locate JSON-LD script block within HTML as borrowed byte span.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Locate JSON-LD script block within HTML as borrowed byte span. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBEXTRACT` in exactly one C11 translation unit. Use `#include "mulib-webextract.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-webextract.c -o /tmp/test-webextract
/tmp/test-webextract
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/webextract.c -o /tmp/example-webextract
```

## API

```c
typedef struct mulib_webextract_span {const char *data;size_t length;} mulib_webextract_span_t;
int mulib_webextract_jsonld(const char *html,mulib_webextract_span_t *out);
```

## Safety and ownership

Returns 1 with a borrowed body span for the first script whose quoted `type` is exactly `application/ld+json`, ignoring ASCII case. Both quote styles and whitespace around `=` are supported. Comments and unrelated script/style bodies are skipped; substrings in other attributes do not match. Returns 0 for absence or an unterminated target, leaving output unchanged. Span is not NUL-terminated and remains valid only while the input is alive and unchanged. No allocation, JSON parsing, schema validation, entity decoding or script execution occurs.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webextract.h`](../include/mulib-webextract.h) |
| Implementation switch | `MULIB_IMPL_WEBEXTRACT` |
| Repository-local includes | [`include/detail/mulib-webscan.h`](../include/detail/mulib-webscan.h) |
| Complete example | [`examples/modules/webextract.c`](../examples/modules/webextract.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webextract.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
