# On1x language reference — implemented milestone

## Lexical conventions

Source files use ASCII identifier spelling `[A-Za-z_][A-Za-z0-9_]*` and
UTF-8 text in string literals (embedded NUL and \0 are rejected in this C-string milestone). `#` introduces a line comment.
Whitespace separates tokens. String escapes are `\\`, `\"`, `\n`, `\r`,
`\t`; the lexer recognizes `\0` but semantic analysis rejects it until length-aware strings are implemented. Generated and fallback scanners are tested for identical
token bytes, byte offsets and diagnostics. Lines advance on LF; columns count
bytes, not Unicode code points. CR and tabs are whitespace, not column expansion.

Each source file is limited to 2 MiB (inclusive). The parser permits at most
256 simultaneously active block/expression/type/pattern parsing levels and
expression-tree levels, including anonymous-function bodies. Function bodies count as blocks; parentheses count
toward parsing nesting even though they do not add HIR nodes. Flat binary and
postfix-call chains also obey the tree-depth limit. Excesses are located
compile-time errors. These are compiler input limits, not restrictions on
runtime function recursion. Each file permits at most 2,048 top-level functions
and 2,048 anonymous functions. Closure capture metadata is limited per checked
unit (including imports) to 16,384 entries and 2 MiB of copied capture names.

## Grammar (readable subset)

The core below is supplemented by [typed macros and PPX syntax](metaprogramming.md):
top-level `macro name { rule (...) => expression {...}; }` / `block {...}`, typed
`$capture: SyntaxCategory`, recursive structural patterns and nested `...`, `name!(...)`,
source `@ppx label "path"` registrations, and labeled `@label("name", ...)`
expression/Block payloads and function attributes. Expansion happens after
parsing and before inference. PPX output must become valid core syntax. Typed
[MSP](staging.md) runs stage checks/AOT lowering after PPX and before inference.

```ebnf
unit    = { import | function | extern } ;
import  = 'import' STRING ';' ;
extern  = 'extern' 'fn' ID '(' [ params ] ')' ['->' type] ';' ;
function= 'fn' ID '(' [ params ] ')' ['->' type] block ;
params  = pattern [ ':' type ] { ',' pattern [ ':' type ] } ;
type    = 'i64' | 'bool' | 'string' | 'void' | 'json' | 'json_view'
         | 'sexpr' | 'buffer' | 'code' '<' type '>'
         | '_' | 'fn' '(' [ type { ',' type } ] ')' '->' type
         | 'forall' '(' ID { ',' ID } ')' type | bound_type_variable
        | '(' type ')' | '(' type ',' type { ',' type } ')' ;
block   = '{' { statement } '}' ;
statement = ('let'|'var') ID [ ':' type ] '=' expr ';'
          | 'let' tuple_pattern [ ':' type ] '=' expr ';'
          | ID '=' expr ';'
          | 'return' [ expr ] ';'
          | 'if' expr block [ 'else' block ]
          | 'while' expr block | block | expr ';' ;
expr    = prefix { binary_operator expr } ;
prefix  = ('!'|'-') prefix | postfix ;
postfix = atom { '(' [ expr { ',' expr } ] ')' } ;
atom    = '(' expr ')' | '(' expr ',' expr { ',' expr } ')'
        | literal | ID | lambda | 'quote' '{' expr '}'
        | ('splice' | 'lift') '(' expr ')' ;
lambda  = 'fn' '(' [ params ] ')' [ '->' type ] block ;
pattern = ID | tuple_pattern ;
tuple_pattern = '(' pattern ',' pattern { ',' pattern } ')' ;
```

Binary operators, low to high precedence: `||`, `&&`, `== !=`,
`< > <= >=`, `+ -`, `* / %`. All are left-associative and statically
type-checked. Postfix calls bind tighter than unary operators. Logical operators
short circuit. The callee is evaluated first, then its arguments left-to-right.
Other expression operands also evaluate **left-to-right**; the C11 backend sequences calls
into temporaries rather than relying on C argument-evaluation order.

Global functions generalize at recursive-component boundaries. Ordinary
parameters have one monotype per instance unless explicitly universal, may omit annotations, and `_`
opts into inference in a type position. A resolved `fn main() -> i64` or
`fn main() -> void` must exist. An omitted return annotation still defaults to
`void`, not inference. `let` and `var` types are inferred from their
initializers; annotations are optional. Variables are lexically scoped and
can shadow bindings in outer scopes.

## Function values and structural monotypes

An ordinary top-level function name denotes a first-class value. Types are
structural and exact: two functions with the same parameter/result types have
the same type, independent of their names. `fn(i64, bool) -> string` is a
two-argument function type; `fn() -> void` is a zero-argument procedure type.
Function types can be nested, including in return position:

```on1x
fn increment(x: i64) -> i64 { return x + 1; }
fn identity(f: fn(i64) -> i64) -> fn(i64) -> i64 { return f; }
fn main() {
  var f: fn(i64) -> i64 = increment;
  let apply: fn(fn(i64) -> i64) -> fn(i64) -> i64 = identity;
  println(apply(f)(41));
}
```

Local and ordinary-function parameter annotations are optional; returns may
use `-> _` for inference. Explicit `forall(a) fn(a) -> a` annotations bind named
type variables for universal parameters/values. There is no subtyping or implicit
numeric coercion. Immutable nonexpansive lets, including destructuring, generalize
as described in [the scheme manual](polymorphism.md). Functions are not implicitly curried: partial application
is an arity error. Only `var` bindings can be reassigned, and their monotype
cannot change. Arguments/results may be functions, tuples, `i64`, `bool`, `string`, or
`code<T>`, `sexpr`, `buffer`, or `void` results; `void` parameters and JSON resources/views at any signature depth
are rejected. Function equality, ordering, arithmetic and printing are rejected.

Postfix call syntax accepts an expression callee: `f(x)`, `(f)(x)`, and
`factory()(x)` are supported. Locals and parameters take precedence over global
names, including intrinsics. A scalar binding shadowing a function causes a
non-callable-value error, not a call to the hidden global function. Top-level
intrinsic definitions are rejected. Intrinsics themselves are not first-class
values; an explicitly typed ordinary wrapper can be used instead.

Values can originate from top-level functions or anonymous function expressions.
They have the same structural types; a noncapturing function and a capturing
closure can be assigned to the same `var`. Function code always has static
lifetime; captured environments are reference-counted automatically.

## Anonymous functions and lexical capture

```on1x
fn add_by(amount) -> _ {
  return fn(value) -> _ { return amount + value; };
}
fn main() {
  let add_two = add_by(2);
  println(add_two(40));
  println(fn(x: i64) -> i64 { return x + 1; }(41));
}
```

Anonymous functions use `fn(params) [-> type] { statements }`. The body uses
explicit `return`, just like top-level functions. Omitted parameter annotations
and `_` holes participate in the same monomorphic inference; an omitted return
annotation still means `void`. Creating a function does not execute its body.
Unbound literals and unannotated parameters are monomorphic per instance;
immutable value bindings can introduce local schemes. There are no named local function declarations or recursive `let`
bindings; a new binding is not visible in its own initializer. Calls to existing
top-level recursive functions remain supported.

Free local names capture **immutable values at creation**, including scalar
parameters, strings, tuples and other function values. Copies/returns keep environments
alive beyond the defining scope. Nested closures forward transitive captures;
ordinary local/parameter shadowing still applies. Only names actually used by
the body or its nested functions are captured, in deterministic first-use order.
Top-level functions, intrinsics and direct raw extern callees are not captures.

Capturing a `var` is rejected, even if never reassigned. To take a snapshot,
first use an immutable binding: `let snapshot = counter; let f = fn() -> i64 {
return snapshot; };`. Ordinary mutable locals **inside** a lambda are allowed.
Capturing JSON owners or views is rejected regardless of whether the closure
escapes. A lambda may create/use/drop its own JSON locals normally; no borrowed
owner escapes in that case. Strings still use the existing C-string ABI:
literal-derived strings have static storage and native-tool dynamic text uses a
bounded process-lifetime snapshot pool. Raw foreign strings retain all caller-side
lifetime obligations. Capturing a foreign pointer does not own its text.

Noncapturing functions need no environment allocation. Capturing literals
allocate reference-counted environments. Locals release their references on
lexical exit/return; function parameters borrow references for the call, and
function-valued results own references. Temporaries are released after their
full expression or completed logical operand/condition, including loop tests;
already evaluated enclosing callees/arguments remain alive. Mutable function
assignment retains the new value before releasing the old one, so self-
assignment is valid. Environment destruction releases captured functions
iteratively, not by recursively unwinding a capture chain.

This runtime is single-threaded, with no weak references, tracing GC or foreign
callback interface. Nonrecursive immutable captures cannot construct reference
cycles. Allocation/reference-count failure terminates with exit status 72 and
does not unwind locals, consistent with the other fatal runtime operations.
Rebuild emitted C and runtime together: the versioned closure representation
is an internal ABI, not a stable C extension ABI. See [closure runtime and
ownership](closures.md) for the precise lowering contract.

## Tuples and local destructuring

```on1x
fn swap(pair) -> _ {
  let (a, b) = pair;
  return (b, a);
}
fn main() {
  let (flag, (n, text)): (bool, (i64, string)) = swap(((42, "text"), true));
  println(n); println(text); assert(flag);
}
```

A tuple is a fixed-size ordered product of **at least two** values. `(i64,
bool)` and `(bool, i64)` are distinct types; arity and every element type must
match exactly. Elements can be scalar values, functions or nested tuples.
`(value)` / `(type)` are grouping, not one-element products. Empty tuples/unit
and trailing commas are not supported. `f(a, b)` passes two arguments;
`f((a, b))` passes one tuple argument.

Tuple elements evaluate left-to-right. `let (name, (other, last)) = expr;`
evaluates the initializer **once**, then copies its fields into immutable local
bindings. Pattern shape/arity must match the inferred tuple type. Patterns can
constrain an otherwise unknown parameter type; holes also work in whole-pattern
annotations such as `let (a, b): (_, bool) = value;`. Eligible variables in
nonexpansive immutable initializers generalize independently per leaf:
destructuring does not generalize extracted functions. Duplicate names, including
repeated `_`, are errors. `_` remains an ordinary identifier, **not** a wildcard.

All new names become visible after the initializer, so it can reference shadowed
outer bindings but not the bindings being introduced. Patterns are supported
in local `let` bindings and ordinary function/lambda parameters, not assignments
or `var` bindings. For example, `fn sum((a, b): (i64, i64)) -> i64 { return a + b; }`
accepts one tuple argument, called as `sum((20, 22))`. An omitted parameter
annotation is inferred from the pattern/body/call sites. All parameter names and
pattern leaves share the body scope; duplicate leaves or body redefinitions are
errors. Pattern leaves are immutable and can be captured or returned normally.
Call arguments borrow their fields, while extracted function/tuple bindings
retain independent references and release them on every normal exit/return.
Raw extern parameters require concrete annotations. Tuple patterns explicitly
flatten to scalar C parameters, with no foreign aggregate layout inferred;
see [the extern-pattern ABI contract](polymorphism.md).
There is no refutable matching, constructor/literal pattern, wildcard pattern,
field/index projection or exhaustive `match` yet.

Tuple fields are immutable. A `var` holding a whole tuple may be reassigned to a
value of the same structural type, including self-assignment. Equality,
ordering, arithmetic and printing are not defined for tuples; destructure and
operate on the scalar fields explicitly. JSON owners/views and `void` cannot
be elements, even when nested or only present in an annotation. A function
inside a tuple must still satisfy all function-signature/resource restrictions.

Tuples use C11 records by value, without separate tuple allocation. Copies,
results, temporary values, local bindings and closure captures retain/release
every nested function environment. Destructured function/tuple names own their
references independently of the original tuple. Function parameters borrow
their fields for the call; returns acquire references before lexical cleanup.
An immutable tuple may be captured; a mutable tuple binding may not. String
fields retain the existing raw C-string lifetime obligations. Raw extern
tuple parameters/results are rejected: compiler-owned records do not establish
a foreign aggregate ABI. See the [tuple manual](tuples.md).

## Type holes and unification

```on1x
fn increment(x) -> _ { return x + 1; }
fn apply(f, x) -> _ { return f(x); }
fn same(a, b) -> _ { return a == b; }
fn main() {
  let operation: fn(_) -> _ = increment;
  println(apply(operation, 41));
  println(same("text", "text"));
}
```

Each omitted parameter annotation or `_` introduces a fresh inference variable
at its declaration position. Function bodies, annotations, assignments and all
call sites constrain the same declaration monotype. Imported functions and
mutually recursive functions are included before solving; body order does not
decide which declarations can be referenced. There is no fresh instantiation
at a `let` or call: `identity(1)` and `identity(true)` cannot use one inferred
identity function at two types. An unconstrained parameter/result is an error,
even in an unused function. Add a concrete annotation or constraining use.

Global declarations now generalize after shared recursive-component inference.
Immutable nonexpansive local lets (named or destructured) generalize free
variables not shared with the environment
or weak overloaded operands generalize, and uses receive fresh instances. Mutable
and expansive bindings do not infer schemes. Parameters vary between instances;
explicit `forall` annotations additionally permit multiple types inside one
body. See [schemes and specialization](polymorphism.md) for the value restriction,
rigid universal checking, recursion rules and bounded predicative subset.

Use `-> _` to infer a return type. A function with no return statements has
an implicit `void` result; otherwise return expressions are unified and the
ordinary definite-return check still applies. A missing return annotation
always means `void`, preserving the earlier language behavior. Integer literals
and numeric operators remain `i64`; this is not numeric defaulting or type-class
inference. Overloaded intrinsics such as `println` do not choose a default type
for an unconstrained argument.

The occurs check rejects infinite types such as `fn omega(f) -> _ { return
f(f); }`. Type grounding also enforces default limits of 65,536 inference graph
nodes, 4,194,304 work steps, 257 type levels including the outer declaration
signature, and 16,384 expanded nodes per type. Shared graphs cannot evade the
expanded-size limit. These budgets apply when a unit uses inference holes;
fully annotated units keep their existing checking path.

All holes must be replaced before the ordinary resource/ABI checker admits the
program. Inferred resource parameters/returns, resource aliases, void bindings,
invalid intrinsic operands and missing returns remain errors. Raw extern
signatures cannot omit parameter annotations or contain `_`, even if call sites
would determine a scalar type: inference is not evidence about a C ABI.

`on1xc --types INPUT.on1x [-o OUTPUT]` prints concrete signatures in deterministic
declaration/import order. Raw declarations carry an `extern` prefix. `_` is a
hole only in a type position; it remains an ordinary identifier in expressions
and bindings, not a wildcard pattern.

## Raw foreign calls and control flow

The `extern fn` syntax is deliberately an *unsafe, raw ABI escape hatch*.
Its `i64` type maps to C `int64_t`, `bool` to C `bool`, and `string` to
`const char *`. The definition must exist in separately linked C code with
an exact compatible C ABI, including compatible calling convention. No
implicit conversion from arbitrary MuLIB pointer/`size_t` types is allowed.
An extern name may only be used as the direct named callee, not stored, passed
or returned as a function value. Function-valued extern parameters/returns are
also rejected: this feature does not enable foreign callbacks or a stable
foreign function-pointer ABI. An On1x wrapper around a raw call is first-class
but retains the call's unsafe foreign behavior.

Control flow includes `if/else`, `while`, and explicit `return`, with
conservative definite-return analysis. `break`, `continue`, `for`, and
`match` are reserved for future milestones.

## Current intrinsics

- `println` prints a `bool`, `i64`, or `string`, plus a line break.
- `assert` terminates with exit status 70 on a false condition.
- `json_parse_or_die`, `json_get_or_die`, `json_size`, `json_number`,
  `json_print` are the temporary MuLIB JSON facade.

## Explicitly absent language facilities

No refutable/exhaustive pattern matching, algebraic data types, generics, higher-kinded types,
modules/functors, mutable/resource captures, native coroutines, async,
macro system, polymorphic numeric literals, foreign callbacks, struct/record
literals, `Option` or `Result`, structured error propagation, or production
borrow checker yet. Do not add their names to public supported-feature lists
until they are implemented with tests.
