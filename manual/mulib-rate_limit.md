# Token bucket

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_RATE_LIMIT`. Initialize finite rate >=0, burst >0 and
nonnegative monotonic seconds. Take requires a finite positive cost. Denial can
update refill state; invalid/backward timestamps grant nothing and change
nothing. Caller owns synchronization. [Example](../examples/modules/rate_limit.c),
[contracts](storage-server.md#http-and-policies).

## Token and clock semantics

`init(bucket, rate, burst, now)` requires finite values, rate ≥ 0, burst > 0,
and now ≥ 0. It starts with a full burst and returns 1; invalid input returns
0 without changing the bucket. Rate is tokens per second; fractional tokens
and costs are supported. No resources are allocated and no destructor is needed.

`take(bucket, cost, now)` requires a positive finite cost and a finite timestamp
no earlier than the previous timestamp. Supply a monotonic application clock.
It refills up to burst, updates the timestamp, and returns 1 only when the cost
can be deducted. A valid but denied request still commits the refill and clock
update; invalid arguments leave state unchanged. A zero refill rate allows only
the initial burst. The caller must synchronize shared access.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rate_limit.h`](../include/mulib-rate_limit.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-rate_limit.h
 * @brief Caller-clocked token bucket. Single-threaded; time in seconds.
 * Define MULIB_IMPL_RATE_LIMIT. Invalid/backwards times never grant tokens. */
#ifndef MULIB_RATE_LIMIT_H
#define MULIB_RATE_LIMIT_H
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_rate_limit
  {
    double tokens, burst, rate, updated;
  } mulib_rate_limit_t;
  int mulib_rate_limit_init (mulib_rate_limit_t *bucket, double rate,
                             double burst, double now);
  int mulib_rate_limit_take (mulib_rate_limit_t *bucket, double cost,
                             double now);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rate_limit.h`](../include/mulib-rate_limit.h) |
| Implementation switch | `MULIB_IMPL_RATE_LIMIT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/rate_limit.c`](../examples/modules/rate_limit.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rate_limit.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
