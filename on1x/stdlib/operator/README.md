# `std.operator.utils`

Import the module explicitly:

```on1x
import std.operator.utils as U;

fn add_one(value: i64) -> i64
{
  return value + 1;
}

fn main()
{
  assert(U.basis_operator_apply(add_one, 41) == 42);
}
```

All named functions below use the `basis_operator_` prefix.

| Function suffix | Behavior |
|---|---|
| `apply` | Calls a unary function on a value. |
| `compose` | Returns a unary function that calls the inner function, then the outer function. The outer function is the first argument. |
| `flip` | Returns a binary function with its arguments reversed. |
| `curry` | Turns a function taking a pair into a function that takes the first value and returns a function awaiting the second. |
| `uncurry` | Turns a curried binary function into one that takes a pair. |
| `result_ok`, `result_error` | Construct `Ok` and `Err` respectively. Supply a result type context if the type of the unused branch cannot be inferred. |
| `result_lift` | Turns a unary function into one returning its answer in `Ok`. |
| `result_map` | Applies a function to `Ok`'s payload; propagates `Err` unchanged. |
| `result_chain` | Applies a Result-returning function to `Ok`'s payload; propagates `Err` unchanged. |
| `result_compose` | Composes two Result-returning functions, inner before outer, stopping at the first `Err`. The outer function is the first argument. |

The Result helpers use the same nominal `Result<T, E>` as the automatic
prelude. They do not accept structurally similar variants, and they preserve
the error type `E`. All returned functions are ordinary On1x closures.

`tests/utils.on1x` exercises the named API without the automatic prelude.
