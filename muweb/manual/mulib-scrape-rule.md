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
cc -std=c11 -Wall -Wextra -Iinclude examples/scrape-rule.c -o /tmp/example-scrape-rule
```

## API

```c
typedef struct mulib_scrape_rule {char *selector,*attribute;int all;} mulib_scrape_rule_t;
int mulib_scrape_rule_init(mulib_scrape_rule_t *r,const char *selector,const char *attribute,int all);
void mulib_scrape_rule_free(mulib_scrape_rule_t *r);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
