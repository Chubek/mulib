# mulib-sitemap: Extract next loc entry from XML sitemap bytes without fetching.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Extract next loc entry from XML sitemap bytes without fetching. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SITEMAP` in exactly one C11 translation unit. Use `#include "mulib-sitemap.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-sitemap.c -o /tmp/test-sitemap
/tmp/test-sitemap
cc -std=c11 -Wall -Wextra -Iinclude examples/sitemap.c -o /tmp/example-sitemap
```

## API

```c
/* Parse next <loc>...</loc> starting at *offset; plain XML subset, no namespace or entity decoding. */
int mulib_sitemap_next(const char *xml,size_t len,size_t *offset,char *url,size_t cap);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
