# mulib-scrape-rule: Owned declarative selector/property pair for extraction rules.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Owned declarative selector/property pair for extraction rules. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SCRAPE_RULE` in exactly one C11 translation unit. Use `#include "mulib-scrape-rule.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-scrape-rule.c -o /tmp/test-scrape-rule
/tmp/test-scrape-rule
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/scrape-rule.c -o /tmp/example-scrape-rule
```

## API

```c
typedef struct mulib_scrape_rule {char *selector,*attribute;int all;} mulib_scrape_rule_t;
int mulib_scrape_rule_init(mulib_scrape_rule_t *r,const char *selector,const char *attribute,int all);
void mulib_scrape_rule_free(mulib_scrape_rule_t *r);
```

## Safety and ownership

Start zero-initialized. `init` copies/replaces a nonempty selector of up to 8192 bytes and optional attribute of up to 1024 bytes. NULL attribute denotes text; empty attribute is stored as empty. `all` is normalized to Boolean. Returns 1 success, 0 invalid/allocation failure with old state unchanged; aliases of old strings are safe. `free` resets state and may be repeated. Do not shallow-copy ownership. This is declarative storage, not selector validation or an extraction engine.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-scrape-rule.h`](../include/mulib-scrape-rule.h) |
| Implementation switch | `MULIB_IMPL_SCRAPE_RULE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/scrape-rule.c`](../examples/modules/scrape-rule.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/scrape-rule.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
