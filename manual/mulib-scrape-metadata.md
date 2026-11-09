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
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/scrape-metadata.c -o /tmp/example-scrape-metadata
```

## API

```c
/* Extract <meta name="key" content="value"> in a conservative, quoted-attribute subset. */
int mulib_scrape_metadata_get(const char *html,const char *key,char *out,size_t cap);
```

## Safety and ownership

Copies the first meta element with a matching quoted `name` and quoted `content`. Tag/attribute/key names are ASCII case-insensitive; attribute order, whitespace around `=`, either quote style and `>` inside quoted values are supported. Exact names prevent matches on `metadata` or `data-name`. Comments and script/style contents are ignored. Returns 1 copied, 0 absent/error/capacity failure with output unchanged. Capacity includes NUL; no overlap. Entities remain encoded; unquoted target values, HTML5 recovery, namespaces, DOM/CSS querying and sanitization are outside this scanner's scope.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-scrape-metadata.h`](../include/mulib-scrape-metadata.h) |
| Implementation switch | `MULIB_IMPL_SCRAPE_METADATA` |
| Repository-local includes | [`include/detail/mulib-webscan.h`](../include/detail/mulib-webscan.h) |
| Complete example | [`examples/modules/scrape-metadata.c`](../examples/modules/scrape-metadata.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/scrape-metadata.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
