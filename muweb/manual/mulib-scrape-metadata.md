# mulib-scrape-metadata: Extract bounded HTML meta content attributes by name.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Extract bounded HTML meta content attributes by name. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SCRAPE_METADATA` in exactly one C11 translation unit. Use `#include "mulib-scrape-metadata.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-scrape-metadata.c -o /tmp/test-scrape-metadata
/tmp/test-scrape-metadata
cc -std=c11 -Wall -Wextra -Iinclude examples/scrape-metadata.c -o /tmp/example-scrape-metadata
```

## API

```c
/* Extract <meta name="key" content="value"> in a conservative, quoted-attribute subset. */
int mulib_scrape_metadata_get(const char *html,const char *key,char *out,size_t cap);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
