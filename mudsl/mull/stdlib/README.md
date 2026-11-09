# MuLL standard-library catalogue

This directory contains discoverable `Package.mull` files. The five native base
packages (`printf`, `terminal`, `math`, `text`, `list`) are installed with
`mull::stdlib::install` / `install`; their implementations are in `../native`.
The optional `Mulib` provider lives in `../bridge/Mulib`. `Stdlib.hpp` and
`Stdlib.d` remain the aggregate installer entry points.

The **30 additional packages** below are implemented in MuLL and shared by C++
and D. Import them through `G:LoadLib(name) or G:DieWith("$E?")`. Dependencies
use the same loader, so the package search root must contain the complete
standard library. Packages return records; functions have fixed arity.

## Composition and data

| Package | API | Contract |
| --- | --- | --- |
| `option` | `Some(x)`, `None()`, `GetOr(o,fallback)`, `Map(o,f)`, `Bind(o,f)` | Records `{present,value}`; `Some(nil)` is present. `Map` wraps the callback result; `Bind` returns it directly. Fallback is an eagerly evaluated argument. |
| `result` | `Ok(x)`, `Err(e)`, `Map(r,f)`, `Bind(r,f)`, `Unwrap(r)` | Records `{ok,value,error}`. Failed results bypass callbacks; unwrap calls `DieWith(error)`. No exception-catching behavior. |
| `function` | `Identity(x)`, `Constant(x)`, `Compose(f,g)`, `Flip(f)`, `Partial(f,a)`, `Times(n,f,x)` | Compose is `f(g(x))`; flip/partial operate on binary functions. Times iterates a unary function with nonnegative integral n. |
| `predicate` | `Negate(p)`, `Both(p,q)`, `Either(p,q)`, `All(xs,p)`, `Any(xs,p)` | Both/either preserve operand-valued short-circuit results; all/any return Booleans. Empty all is true, empty any false. |
| `pair` | `New(a,b)`, `First(p)`, `Second(p)`, `Swap(p)`, `Map(p,f,g)` | Pairs are two-element lists; consumers check length. |
| `sequence` | `Zip(a,b)`, `Enumerate(xs)`, `Flatten(xs)`, `Reverse(xs)` | Zip truncates to the shorter list; enumeration starts at zero; flatten removes one list level. |
| `slice` | `Sub(xs,start,end)`, `Take(xs,n)`, `Drop(xs,n)`, `Chunk(xs,n)` | Half-open bounds must be integral and in range. Chunk size must be positive; final chunk can be short. |
| `search` | `Index(xs,value)`, `Contains(xs,value)`, `Find(xs,p)`, `Count(xs,p)` | Index returns nil when absent. Find returns `{found,index,value}`, distinguishing a matched nil. Structural equality; first match wins. |
| `sort` | `By(xs,less)`, `Ascending(xs)`, `Descending(xs)` | Stable insertion sort. Comparator must define a strict ordering. Numeric ascending/descending; input remains unchanged. |
| `set` | `Unique(xs)`, `Union(a,b)`, `Intersection(a,b)`, `Difference(a,b)` | Lists represent sets with structural equality, unique results, and stable first-occurrence order. |
| `stack` | `New()`, `Push(xs,x)`, `Peek(xs)`, `Pop(xs)` | Last element is top; pop returns `{value,rest}`. Peek/pop reject empty input. |
| `queue` | `New()`, `Enqueue(xs,x)`, `Peek(xs)`, `Dequeue(xs)` | First element is front; dequeue returns `{value,rest}`. Peek/dequeue reject empty input. |
| `deque` | `New()`, `PushFront(xs,x)`, `PushBack(xs,x)`, `PopFront(xs)`, `PopBack(xs)` | Functional double-ended queue using the same removal record as stack/queue. |

## Numbers and collections

| Package | API | Contract |
| --- | --- | --- |
| `vector` | `Dot(a,b)`, `Scale(a,k)`, `Norm(a)`, `Add(a,b)`, `Normalize(a)` | Numeric lists; binary operations require equal lengths. Empty dot is zero; normalization rejects zero norm. |
| `matrix` | `Shape(m)`, `Transpose(m)`, `Multiply(a,b)` | Rectangular lists of rows. Shape is `[rows,columns]`; `[]` has shape `[0,0]`. Multiply requires compatible dimensions and numeric entries. An empty outer list cannot preserve a nonzero column count. |
| `statistics` | `Sum(xs)`, `Mean(xs)`, `Variance(xs)`, `Median(xs)` | Numeric lists; variance is population variance (divide by n), median averages the middle pair. Sum of empty is zero; the other operations reject empty lists. Binary64 rounding applies. |
| `integer` | `Check(x)`, `Gcd(a,b)`, `Lcm(a,b)`, `DivMod(a,b)`, `IsEven(x)` | Integral inputs within ±(2^53−1); LCM also checks its result. GCD/LCM are nonnegative; gcd(0,0)=0. DivMod uses floor quotient, with remainder having divisor's sign; zero divisor rejected. |
| `bit` | `And(a,b)`, `Or(a,b)`, `Xor(a,b)`, `Not(a)`, `ShiftLeft(a,n)`, `ShiftRight(a,n)` | Unsigned 32-bit inputs; shift counts 0–32, integral. Left shift wraps modulo 2^32; right shift is logical; shifting by 32 returns zero. |
| `combinatorics` | `Factorial(n)`, `Choose(n,k)`, `Cartesian(a,b)` | Factorial accepts 0–18; choose accepts 0≤k≤n≤50, keeping intermediates exact. Cartesian order is left-list-major; an empty input yields empty output. |
| `range` | `Step(start,end,step)`, `Contains(start,end,x)`, `Clamp(x,low,high)` | Step uses checked exact integers and nonzero signed step; end excluded; a direction mismatch gives empty. Contains is half-open; clamp requires low≤high. |

## Strings and application utilities

| Package | API | Contract |
| --- | --- | --- |
| `string` | `Sub(s,start,end)`, `StartsWith(s,p)`, `EndsWith(s,p)`, `Trim(s)`, `Repeat(s,n)` | Byte indexing; checked integral half-open bounds. Trim removes space/TAB/CR/LF. Empty prefix/suffix match. Repeat count is nonnegative integral. |
| `ascii` | `Digits`, `Upper`, `Lower`, `IsDigit(c)`, `IsAlpha(c)`, `IsAlnum(c)`, `IsSpace(c)`, `Digit(c)` | Predicates require one byte. Space means space/TAB/CR/LF. Digit accepts decimal ASCII digits and returns 0–9. |
| `csv` | `Encode(rows)`, `Decode(s)` | String fields, comma delimiter, doubled quote escaping. Decode accepts LF/CR/CRLF and embedded quoted newlines, rejects unclosed or misplaced quotes. Encode quotes every field and terminates each nonempty row with LF. Empty document is `[]`; an empty field is `""`. |
| `path` | `Normalize(p)`, `Join(a,b)`, `Basename(p)`, `Dirname(p)` | POSIX lexical operations only. Dot segments and repeated slashes removed; unresolved relative `..` preserved; absolute paths stay rooted. Empty normalizes to `.`; absolute b replaces a when joining. |
| `url` | `Encode(s)`, `Decode(s)` | Percent coding for printable ASCII plus TAB/CR/LF. Unreserved characters pass through, plus is literal, decode accepts either hex case. Truncated/malformed escapes and unsupported bytes are errors; this is not a full URL parser or Unicode codec. |
| `semver` | `Parse(s)`, `Compare(a,b)`, `Compatible(a,b)` | Strict `major.minor.patch` core versions, exact integers, no leading zeros/suffixes. Compare returns −1/0/1. Compatible means same nonzero major, or same nonzero minor under major zero, or identical patch under 0.0; it is symmetric, not a range query. |
| `units` | `Radians(degrees)`, `Degrees(radians)`, `CelsiusToFahrenheit(c)`, `FahrenheitToCelsius(f)`, `MetersToFeet(m)`, `FeetToMeters(f)`, `SecondsToMilliseconds(s)`, `MillisecondsToSeconds(ms)` | Scalar binary64 conversions; one foot is exactly 0.3048 meters by definition. |
| `geometry` | `Point(x,y)`, `Rect(x,y,w,h)`, `Distance(a,b)`, `Area(r)`, `Contains(r,p)`, `Intersection(a,b)` | Records with x/y and width/height. Nonnegative extents; right/bottom edges excluded. Disjoint intersection has zero extent. |
| `color` | `Rgb(r,g,b)`, `Hex(c)`, `Mix(a,b,t)`, `Invert(c)` | Integral channels 0–255; uppercase `#RRGGBB`. Mix uses 0≤t≤1 with nearest-integer rounding; no gamma correction. |
| `assert` | `Check(condition,message)`, `Equal(a,b)`, `NotEqual(a,b)`, `Near(a,b,tolerance)` | Failing assertions throw through `G:DieWith`/`G:Assert`. Near uses nonnegative absolute tolerance. Successful checks return the first value/condition. |

## Cost and execution

These packages favor small, portable algorithms over specialized native data
structures. Lists and strings are immutable; repeated append copies accumulated
values. Consequently many builders are quadratic in output length, sets use
linear membership scans, and insertion sort has quadratic comparisons (with
additional copying costs). Matrix multiplication performs the usual cubic
scalar work for square inputs. Use native packages for large data workloads.
All MuLL loops/callbacks share the runtime's step, call-depth, frame, and
collection-depth limits. Invalid preconditions raise an error; they are not
silently clamped unless the API is explicitly `Clamp`.

`tests/mull/expanded-stdlib.mull` covers every package. Additional fixtures
exercise malformed CSV, ragged matrices, invalid slices, and invalid versions.
