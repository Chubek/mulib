# Enum connective

Use `value ~ { pattern => expression, … }` to match a closed sum:

```on1x
enum Result<T, E> { Ok(T), Err(E) }

fn report(result: Result<i64, i64>) -> i64 {
  return result ~ {
    Ok(-) => 0,
    Err(code) if code < 0 => 1,
    Err(code) => code,
  };
}
```

The subject must already have a known closed sum shape in inference: either a
nominal `enum` or an explicit structural `sum{…}`. The arm labels do not infer
an enum or define a new sum type. This is not shorthand for matching arbitrary
scalars, tuples or records; use `match value with { … }` for those.

The brace form uses the same patterns and checks as `match … with`. A lone `-`
is another spelling of `_` in its patterns, including inside payload patterns;
`-2` and `-0.0` are still signed literals. Outside this form, `-` is not a
pattern wildcard. A payloadless constructor is matched as `Name()`.

The subject is evaluated once. Arms are tried in source order. A guard runs
only when its pattern matches, can use that pattern’s bindings, and must return
`bool`. A false guard continues to the next arm. Arms must be exhaustive;
nontrivial guards do not prove coverage, and redundant arms are rejected.
Expression arms and value blocks follow ordinary match result typing. As an
expression statement, terminate the connective with `;`.

The brace form has equality precedence, below arithmetic and ordering
comparisons and above `&&` and `||`. For example, write
`1 + (value ~ { Ok(n) => n, Err(-) => 0 })` to add to the match result.
The brace immediately after `~` selects this syntax, not the string/regex form.

The connective uses existing checked match lowering to C11; it does not extend
the scalar-only MIR profile to sums.
