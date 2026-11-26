# `std.operators`

The automatic prelude makes these operators visible. With `--no-prelude`,
select only the operators you use:

```on1x
from std.operators import operator("|>");

fn add_one(value: i64) -> i64
{
  return value + 1;
}

fn main()
{
  assert((41 |> add_one) == 42);
}
```

| Expression | Meaning | Precedence | Associates |
|---|---|---:|---|
| `x \|> f` | `f(x)` | 1 | Left |
| `f <\| x` | `f(x)` | 1 | Right |
| `f >>. g` | A function that calls `f` then `g` | 7 | Left |
| `g <<. f` | A function that calls `f` then `g` | 7 | Right |
| `f <$> r` | Map `f` over the success of `r` | 2 | Left |
| `r <&> f` | Map `f` over the success of `r` | 2 | Left |
| `r >>= f` | Chain `r` into the Result-returning function `f` | 0 | Left |
| `f =<< r` | Chain `r` into the Result-returning function `f` | 0 | Right |
| `f >=> g` | Compose Result-returning functions, `f` before `g` | 7 | Left |
| `g <=< f` | Compose Result-returning functions, `f` before `g` | 7 | Right |

All ten start with tightness 0.

The Result operators use the prelude's nominal `Result<T, E>`. Mapping
changes only an `Ok` payload; binding and effectful composition stop at the
first `Err` and preserve its payload and type. The function operand itself
is evaluated normally; an `Err` skips invoking the function.

Named counterparts are available from [`std.operator.utils`](../operator/README.md).

`tests/pervasive.on1x` exercises the automatic prelude. Run `tests/selective.on1x`
with `--no-prelude` to exercise explicit selection. With the same flag,
`tests/selective_hidden.on1x` must reject an unselected operator and
`tests/nominal_rejected.on1x` must reject a different nominal sum.
