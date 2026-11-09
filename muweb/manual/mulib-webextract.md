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
cc -std=c11 -Wall -Wextra -Iinclude examples/webextract.c -o /tmp/example-webextract
```

## API

```c
typedef struct mulib_webextract_span {const char *data;size_t length;} mulib_webextract_span_t;
int mulib_webextract_jsonld(const char *html,mulib_webextract_span_t *out);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
