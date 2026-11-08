# MuLIB date

[Manuals](README.md) · [Library index](index.md)

Gregorian leap-year and calendar-days arithmetic. Uses the proleptic Gregorian calendar for integer years; does not model time zones, leap seconds, or historical calendar transitions.

## Contract and behavior

Month and day numbers are one-based. `days_in_month()` returns 0 for
months outside 1..12; `valid()` checks the day against the month's length.
Leap-year arithmetic follows the Gregorian divisible-by-4/100/400 rules for
the supplied integer year, without enforcing a historical adoption date or
positive-year range. These are allocation-free predicates/value helpers, not
timestamp, timezone, or calendar-conversion functions.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_DATE
#include "mulib-date.h"
int main(void) {
    assert(mulib_date_is_leap(2024) && mulib_date_valid(2024, 2, 29));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-date.h`](../include/mulib-date.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-date.h
 * @brief Gregorian leap-year and calendar-days arithmetic.
 * @defgroup mulib_date MuLIB date
 * @{
 * @details Uses the proleptic Gregorian calendar for integer years; does not
 * model time zones, leap seconds, or historical calendar transitions.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DATE before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_DATE_H
#define MULIB_DATE_H
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

  /** @brief mulib date is leap; see manual/mulib-date.md for contracts and
   * examples. */
  int mulib_date_is_leap (int year);
  /** @brief mulib date days in month; see manual/mulib-date.md for contracts
   * and examples. */
  unsigned mulib_date_days_in_month (int year, unsigned month);
  /** @brief mulib date valid; see manual/mulib-date.md for contracts and
   * examples. */
  int mulib_date_valid (int year, unsigned month, unsigned day);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DATE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-date.h`](../include/mulib-date.h) |
| Implementation switch | `MULIB_IMPL_DATE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/date.c`](../examples/modules/date.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/date.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
