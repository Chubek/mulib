# MuLL language and embedding reference

MuLL is a dynamically typed, lexically scoped tree interpreter. Its complete
lexer, parser, value model, environments, package loader, and evaluator live
in `MuDSL.hpp` and `MuDSL.d`. Standard-library implementations live here, outside
the core. C++ needs C++20 and the STL; D needs Phobos.

## Embed

```cpp
#include "MuDSL.hpp"
#include "mull/stdlib/Stdlib.hpp"

mull::Runtime vm({"/absolute/path/to/mull/stdlib"});
mull::stdlib::install(vm);
vm.stdoutSink = [](std::string_view text) { std::cout << text; };
auto result = vm.run("return 6 * 7;", "example.mull");
assert(result.number() == 42);
```

```d
import MuDSL;
import mull.stdlib.Stdlib;

auto vm = new MuLL.Runtime(["/absolute/path/to/mull/stdlib"]);
install(vm);
auto result = vm.run("return 6 * 7;", "example.mull");
assert(result.number() == 42);
```

Compile the complete D example from the project root:

```sh
ldc2 -I. examples/27-mull.d MuDSL.d mull/stdlib/Stdlib.d \
  mull/stdlib/*/Native.d -of=mull-example
```

Runtime methods in both implementations:

| Method | Meaning |
| --- | --- |
| `run(source, name = "<input>")` | Parse all source, then evaluate; return its result. |
| `runFile(path)` | Bounded file read followed by `run`. |
| `define(name, value, mutableBinding = false)` | Add a host global; duplicate names throw. |
| `get(name)` | Retrieve the current value of a global binding. |
| `registerLibrary(name, value)` | Register a native package implementation. |
| `addLibraryPath(path)` | Append a discovery root. |
| `loadLibrary(name)` | Import a package; return nil on failure. |
| `lastError()` | Last import diagnostic; successful imports clear it. |

The constructor takes an ordered root list, defaulting to `["mull/stdlib"]`.
Pass an empty list to start without discovery roots. Installed package files
are under `<prefix>/share/mull/stdlib`; pass that path explicitly.

Use `mull::Value` / `MuLL.Value` for host values. C++ `Value::Native` is
`std::function<Value(const Value::List&)>`; D `MuLLValue.Native` is a delegate
taking `MuLLValue[]` and returning `MuLLValue`. `arity` / `mullArity` validates
argument counts. Accessors include `number`, `list`, `object`, `member`, `call`,
`text`, `type`, and `truth`. String access is `string()` in C++ and
`stringValue()` in D. A default value is nil.

C++ script callables borrow their runtime: keep it alive until their last call.
Runtime objects cannot be copied or moved. D uses garbage-collected environments.
Each instance is single-threaded; independent instances have independent state.
Reentering `run` from a native callback throws. Native callbacks can invoke a
function argument with `Value.call` instead.

## Syntax

```mull
val constant <- 10;
var count <- 0;
ref alias <- count;
alias <- alias + 1;
val square <- fn(x) { return x*x; };
if count < constant { count <- square(count); }
else { count <- 0; }
while count < 5 { count <- count + 1; }
```

`val` is immutable; `var` is mutable; `ref` aliases an existing mutable cell.
References require an identifier initializer. Parameters are mutable local
bindings initialized by value; sharing mutation uses captures, not ref parameters.
`<-` also assigns to an existing mutable name. Assignment is a statement.
Braces introduce scopes; same-scope duplicate declarations are errors.

Statements require semicolons except blocks, `if`, and `while`.
Conditions may be parenthesized. `else if` is supported. `break;` exits a loop
but cannot cross a function boundary. `return expression;` returns immediately;
`return;` returns nil. At top level, return ends the run or package. Otherwise
the last statement value is the result; an empty program/block returns nil.
Top-level bindings persist across runs. Closures retain live lexical environments,
including subsequent assignments/declarations. Recursion is supported, without
tail-call elimination.

Identifiers are case-sensitive ASCII `[A-Za-z_][A-Za-z_0-9]*`. Keywords are
`val var ref fn if else while return break and or not true false nil`.
Comments start with `#` or `//` and extend to the newline. Strings can contain
arbitrary bytes; whitespace outside strings is space, tab, CR, or LF.

### Values

- `nil`, Booleans, finite binary64 numbers, byte strings, lists, records, functions.
- Only **nil and false** are falsey. Zero and empty collections are true.
- Decimal numbers accept a fractional part and exponent. Overflow and nonzero
  literals rounding to zero are errors; arithmetic underflow follows binary64.
- Strings: `"text"`, with `\n \r \t \0 \\ \"` escapes. Indexing and length
  are byte-based. Unknown escapes are errors.
- `$E?` inside a string literal expands once to the last import error when
  evaluated. Other interpolation is supplied by `printf:Format`.
- Lists: `[1, "two", [3]]`; `xs[0]` retrieves an element.
- Records: `{name: "MuLL", "other-key": 1}`; `r:name` or `r["name"]`.
- Collections are structurally immutable; library updates return new values.
  Indexes must be nonnegative integral numbers in range. Missing keys throw.
- Equality is structural, with different kinds unequal and functions compared
  by identity. There is no implicit numeric/string coercion.
- `object:Method(args)` selects a member then calls it, **without** an implicit
  receiver argument. Methods capture state when needed.

### Operators (lowest precedence first)

| Operators | Meaning |
| --- | --- |
| `or` | Return left if true, otherwise evaluate and return right. |
| `and` | Return left if false, otherwise evaluate and return right. |
| `== !=` | Equality and inequality. |
| `< > <= >=` | Numeric ordering. |
| `+ -` | Numeric arithmetic; `+` also concatenates two strings or two lists. |
| `* / %` | Numeric multiplication, division, remainder (`fmod`). |
| unary `+ - ! not` | Numeric sign or Boolean negation. |
| `:name [index] (args)` | Member, index, and call postfixes. |

Binary operators associate left-to-right. Operands and arguments evaluate
left-to-right. `and`/`or` skip unused operands and do not catch exceptions.
Division/remainder by zero and nonfinite numeric results throw.

### Grammar outline

```ebnf
program     = { statement } ;
statement   = ("val" | "var" | "ref") name "<-" expression ";"
            | expression [ "<-" expression ] ";"
            | "return" [ expression ] ";"
            | "break" ";"
            | "if" expression block [ "else" (block | if-statement) ]
            | "while" expression block | block ;
block       = "{" { statement } "}" ;
primary     = number | string | name | "true" | "false" | "nil"
            | "(" expression ")"
            | "[" [ expression { "," expression } ] "]"
            | "{" [ key ":" expression { "," key ":" expression } ] "}"
            | "fn" "(" [ name { "," name } ] ")" block ;
key         = name | string ;
```

Expressions combine primaries with the precedence table above. Trailing commas
are not accepted. Parenthesize a bare record expression at statement start to
distinguish it from a block. Assignment/ref targets receive semantic checks.

## Host record `G`

`LoadLib(name)`, `Native(name)`, `GetStdout()`, `DieWith(value)`, `Assert(value)`,
`Str(value)`, `Type(value)`, and `Len(value)` are the core services.
`GetStdout()` returns a callable accepting one string, following the host's
current `stdoutSink`. `Len` counts string bytes/list elements/record entries.
`Str` produces display text, not a serialization format; numbers use 17
significant digits and record keys display in sorted order.

## Packages

Discovery searches `<root>/<name>/Package.mull` in root order. Names contain only
ASCII letters, digits, underscores, and hyphens. A package runs in an isolated
environment containing `G` and must return a record. It can define private
bindings and return closures. Successful results are cached per runtime/name;
failed imports can be retried, and recursive imports are detected. Once a file
is found, its failure is reported rather than falling through to another root.

The supplied `Package.mull` files return `G:Native("name")`. Their native records
must first be registered by `mull::stdlib::install(vm)` / `install(vm)` or the
individual library installer. Registration compiles into the application;
there is no runtime compilation or dynamic shared-library ABI.

```mull
val Printf <- G:LoadLib("printf") or G:DieWith("Import Error: $E?");
Printf:SetMainSink(G:GetStdout());
Printf:Println("ready");
```

`LoadLib` converts discovery/parse/evaluation exceptions to nil and records their
message. Success clears that message, including cache hits. Other runtime errors
throw `mull::Error` / `MuLL.Error`, with source name, line, and byte column.
Syntax errors occur before any effects. Runtime failures preserve earlier effects.

## Standard library

| Package | Members |
| --- | --- |
| `printf` | `SetMainSink(fn)`, `Format(template, ...)`, `Printf(template, ...)`, `Print(...)`, `Println(...)` |
| `terminal` | `Write(string)`, `Clear()`, `Color(code, string)` |
| `math` | `Abs(x)`, `Floor(x)`, `Ceil(x)`, `Sqrt(x)`, `Pow(x,y)`, `Min(x,y)`, `Max(x,y)`, `Pi` |
| `text` | `Split(text, sep)`, `Join(strings, sep)`, `Replace(text, from, to)`, `Upper(text)`, `Lower(text)` |
| `list` | `Range(start,end)`, `Map(xs,fn)`, `Filter(xs,fn)`, `Fold(xs,initial,fn)`, `Push(xs,value)`, `Set(xs,index,value)` |

- Formatting uses `{}` placeholders, `{{`/`}}` for literal braces, and exact
  argument counts. `Print`/`Println` concatenate without separators. Output
  functions return nil; `Format` returns a string. Sinks receive one string.
  Recursive writes through the same package sink raise an error; a sink can
  replace itself for subsequent writes.
- Terminal functions use the runtime stdout callback directly; `Color` returns
  an ANSI SGR-wrapped string for integral codes 0–107. `Clear` writes ANSI bytes.
- Text split preserves empty fields. Empty split separators/replacement search
  strings throw. Replace is nonoverlapping. Case conversion is ASCII-only.
- Range is integral, ascending, end-exclusive, within ±(2^53−1), with at most
  1,000,000 elements. Map/filter callbacks receive one element; fold callbacks
  receive accumulator then element. Processing is in list order.

## Limits and lifetime

Host-configurable defaults: `maxSteps=1000000`, `maxCallDepth=128`,
`maxFrames=100000`, `maxSourceBytes=4194304`. `run` resets steps; nested imports
and script callbacks share that budget. Frames count cumulatively for a runtime.
The C++ environment arena is freed on runtime destruction; D can collect
unreachable frames but enforces the same allocation count. Parser nesting and
operator chains and collection nesting are limited to 256; evaluator nesting is limited to 512.
Limits count interpreter work, not native callback time or arbitrary host memory.

The language has no classes, catch syntax, mutable collection elements, or
compiler backend. Extend services using native packages and the same value API.

## Verification

CTest runs toolkit regressions, embedding tests, and shared `.mull` fixtures
against both implementations when a D compiler is available. Complete MuLL code
blocks in book chapters 46–70 are also executed and compared. See
`scripts/check_mull.py` and `tests/mull/`.
