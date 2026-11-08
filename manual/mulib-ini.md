# MuLIB ini

[Manuals](README.md) · [Library index](index.md)

Callback-based INI section and key-value line parser. Accepts [section] headings and key=value pairs, ignores comments beginning with # or ; at the start of a trimmed line, and stops when callback returns zero. Quoted multiline values and escape semantics are outside this format subset.

## Contract and behavior

Input is a NUL-terminated string split on LF, with surrounding whitespace
trimmed. Blank lines and whole-line `;`/`#` comments are ignored. Sections use
`[name]`; assignments split at the first `=`. Keys before a section use an empty
section name. Quotes and inline comment characters in values remain literal;
there is no interpolation or continuation syntax.

The callback receives borrowed section/key/value strings that must be copied
if retained. Return nonzero to continue. Parse returns 1 on completion and 0
on malformed input, allocation failure, or callback rejection. Prior callback
effects remain after failure; duplicate keys are delivered in source order.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_INI
#include "mulib-ini.h"
static int mulib_example_ini_cb(void *u, const char *section, const char *key,
                                const char *val) {
    (void)u;
    return strcmp(section, "server") == 0 && strcmp(key, "port") == 0 &&
           strcmp(val, "1234") == 0;
}
int main(void) {
    assert(
        mulib_ini_parse("[server]\nport=1234\n", mulib_example_ini_cb, NULL));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ini.h`](../include/mulib-ini.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-ini.h
 * @brief Callback-based INI section and key-value line parser.
 * @defgroup mulib_ini MuLIB ini
 * @{
 * @details Accepts [section] headings and key=value pairs, ignores comments
 * beginning with # or ; at the start of a trimmed line, and stops when
 * callback returns zero. Quoted multiline values and escape semantics are
 * outside this format subset.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_INI before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_INI_H
#define MULIB_INI_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Callback-based INI section and key-value line parser. */
  typedef int (*mulib_ini_callback) (void *user, const char *section,
                                     const char *key, const char *value);
  /** @brief mulib ini parse; see manual/mulib-ini.md for contracts and
   * examples. */
  int mulib_ini_parse (const char *source, mulib_ini_callback callback,
                       void *user);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_INI_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ini.h`](../include/mulib-ini.h) |
| Implementation switch | `MULIB_IMPL_INI` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ini.c`](../examples/modules/ini.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ini.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
