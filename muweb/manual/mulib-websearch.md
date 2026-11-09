# mulib-websearch: Search provider identification and URL routing for Google/Bing browser searches.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Search provider identification and URL routing for Google/Bing browser searches. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBSEARCH` in exactly one C11 translation unit. Use `#include "mulib-websearch.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-websearch.c -o /tmp/test-websearch
/tmp/test-websearch
cc -std=c11 -Wall -Wextra -Iinclude examples/websearch.c -o /tmp/example-websearch
```

## API

```c
typedef enum mulib_websearch_provider {MULIB_WEBSEARCH_GOOGLE=1,MULIB_WEBSEARCH_BING=2} mulib_websearch_provider_t;
const char *mulib_websearch_provider_name(mulib_websearch_provider_t p);
int mulib_websearch_known(mulib_websearch_provider_t p);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
