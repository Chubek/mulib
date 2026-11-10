# mulib-texruntime — bounded recursive numeric functions

This header implements a small interpreted expression language capable of **named functions and recursion**. It is a useful, executable step beyond mere text macros, but **it is not yet a general-purpose document language or TeX engine**.

## Public API

```c
int mulib_texruntime_eval(const char *program, size_t max_steps,
                          unsigned max_depth, double *out);
```

Returns 1 on successful evaluation. Non-finite numeric results, syntax errors, unknown variables/functions, divide-by-zero, recursion over depth, and execution over budget return 0; `out` is preserved on failure. `max_steps=0` defaults to 100000 and `max_depth=0` defaults to 64 (maximum 256).

Syntax: zero or more `fn NAME(PARAM) = EXPR;` declarations followed by an optional `eval` keyword and one arithmetic expression. Expressions support finite decimal numbers, one parameter per function, named function invocation, parentheses, unary signs, addition/subtraction/multiplication/division, `<`, `<=`, `>`, `>=`, `==`, `!=`, and lazy `?:` branches.

```text
fn fib(n) = n <= 1 ? n : fib(n-1) + fib(n-2);
eval fib(12)
```

Missing language facilities include function values, lexical closures, lists/maps/strings, effects, file I/O, modules, mutable state, AST macros, and document node construction. Provide those before claiming full Turing-completeness in the ordinary language-implementation sense. The language is recursively expressive with deliberately bounded resources.

Compile:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texruntime.c -lm -o demo
./demo
```

Hardening: create token/AST IR so expressions are parsed once, implement a real call stack and closures, differential-test against AST evaluators, add scanner/source locations, fuzz malformed programs and very deep nested expressions, integrate resource capability controls and DocIR constructors.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-texruntime.h`](../include/mulib-texruntime.h) |
| Implementation switch | `MULIB_IMPL_TEXRUNTIME` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texruntime.c`](../examples/mulib-texruntime.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texruntime.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
