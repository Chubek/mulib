# New Syntactic Additions to On1x

**Status:** partially implemented specification; bounded C11 profiles execute, but the full specification is **not complete**.
**Target:** `Chubek/mulib/on1x`, with optional facilities from `on1x/third_party/libstone`.  
**Scope:** intrinsic On1x syntax and semantics, **not** `stdppx`, `stdlib/ppx`, macros, or PPX transforms.

## Implementation ledger — 2026-11-18

Re-inspected repository `d4cc478df76bfe9b5b8e6485f42d56fde81e8a68` and
configured LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`.
The sections below specify the intended language, not blanket compiler support.
The linked profile manuals define what is actually admitted today.
**EXECUTABLE** means a bounded source→checked/canonical HIR→C11 program, not
full-specification conformance or support in scalar MIR. No row is VERIFIED.

| Section / capability | Current admitted status | Remaining specification gap |
|---|---|---|
| §1 structural match | EXECUTABLE bounded value profile; [manual](docs/core-syntax.md) | Owning lists/cons patterns, general recursive/resource patterns |
| §2 internal fibers | EXECUTABLE cooperative i64 profile; [manual](docs/concurrency.md) | General results/captures, running cancellation, native-thread ownership |
| §2 external fibers | EXECUTABLE bounded POSIX argv/backtick commands and byte streams | Full portable shell AST/operators/expansion, typed process outcomes, other targets |
| §2 channels | EXECUTABLE 1:1 i64 uni channels and process-byte endpoints | General channel payloads, duplex, cardinality quotas/cloning, cancellation |
| §2 atomic `poll` | EXECUTABLE i64 plus captured u8 streams, mixed choices; [increment](docs/stream-poll.md) | Monotonic `after`, cancellation, general endpoint expressions and effectful MIR |
| §3 loops/exits/results | EXECUTABLE statement/range/for-C and value loops; [manual](docs/value-loops.md) | General iterator/drop protocol and destructuring iteration |
| §4 cleanup | EXECUTABLE **different bounded profile**: [function-scoped Go-style `defer call;`](docs/defer-errors.md) | This is not the scope-exit `defer` specified below; `using`, scope cleanup, exception/cancel unwind remain unimplemented |
| §5 typed errors | EXECUTABLE nominal Result postfix `?` and explicit match recovery | `try`/`catch`/`finally`/`throw`, Option propagation, general error effects |
| §6 async/await | NOT IMPLEMENTED | Typed suspension IR, liveness, frames, scheduler/ownership/cleanup |
| §7 generators | NOT IMPLEMENTED | Stream ownership, yield/frame/drop semantics and iterator protocol |
| §8 taskgroups | EXECUTABLE bounded lexical scopes, explicit joins and immediate pre-dispatch internal / consuming external cancel | Automatic outstanding-child join, deadlines and running/blocked internal cancel |
| §9 synchronization | NOT IMPLEMENTED as checked language constructs | Shared ownership/memory model, atomic/lock typing and suspension restrictions |
| §10 owning-list comprehensions | NOT IMPLEMENTED | Owning list ABI, builder/drop protocol and allocation outcomes |
| §11 nominal records/enums | EXECUTABLE nonrecursive trivial-value profile; [manual](docs/nominal-adts.md) | Boxed recursive/owning layouts and general constructor sugar |
| §12 contracts/invariants | NOT IMPLEMENTED as intrinsic syntax | Pure contract typing, runtime checks, `old`, verifier obligations |
| §13 canonical Core | LOWERED/checked for admitted control/value regions and CSP operations | Full effect/ownership/suspension Core and MIR; no general source-ASDL migration |

This continuation adds real mixed-byte `poll` execution, tests and an example;
it does not complete G1–G6 or M4. See [verification](VERIFICATION.md) for exact
commands/results and [capability matrix](docs/capability-matrix.md) for source,
ABI and blocker details. The configured LibStone checkout is read-only; MuLIB's
existing channel/fiber kernels own this runtime, not Plaid.

## 0. Architecture, implementation boundaries, and language-wide invariants

On1x currently runs a LibStone Aurocks scanner generated from `on1x/grammar/On1x.g` and the token-arena definition in `on1x/grammar/On1x.asdl`; its actual source parser and semantic HIR are in `on1x/src/parser.cpp` and `on1x/include/on1x/ast.hpp`. Thus **adding AST variants to ASDL alone is insufficient**. Expand the source grammar and its generated representation, or introduce a new versioned source-ASDL tree alongside the existing token arena and migrate the C++ parser incrementally. Preserve the scanner fallback, scanner parity, source offsets, diagnostic limits, macro hygiene, existing PPX AST protocol, and original language behavior.

Existing `match(value){Some(n)=>...,None(unit)=>...}` accepts only exhaustive closed structural sums. Generalized `match` below is an **extension**, not an incompatible replacement. Existing tuple, record, array, slice and numeric types have bounded by-value semantics; ordinary heap lists, interthread captures, `ifiber`/`xfiber`, cancellation, error propagation, and asynchronous resource lifetimes require new runtime and checker work. The C11 emitter remains the execution path; the current scalar MIR v1–v3 is intentionally a restricted subset, not a production concurrency IR. Do not claim all backends support a feature simply because one emitter does.

### Required front-end and lowering stages

1. **Lex and parse** into source AST nodes that preserve all syntax and source locations; parse errors must be located and bounded.
2. **Resolve names, macros and PPX** without allowing PPX to manufacture trusted types, ownership, capabilities, or already-lowered nodes. Update the versioned PPX wire format, or expand source constructs before that boundary without changing their meaning.
3. **Infer and elaborate types/effects,** check control flow, arm exhaustiveness, loop targets, resource ownership, cancellation boundaries, and thread safety.
4. **Canonicalize** each surface construct to a small, typed, backend-neutral `Core` with explicit blocks, branches, temporaries, CFG edges, structured exits, suspension sites, and intrinsic effects. `match`, `poll`, `for`, `defer`, etc. do not survive as surface nodes.
5. **Verify Core** independently: dominance, initialized locals, balanced cleanup, effect tokens, target capabilities, complete terminators, and source provenance.
6. **Lower Core** to verified MIR or the C11 emitter and runtime adapters. If a target lacks the necessary operation, issue a precise *unsupported target/profile* error; do not silently drop the operation or use a behaviorally different fallback.

**Evaluation order:** left-to-right; a scrutinee, guard operand, channel expression, send payload, loop bound, and command interpolation are evaluated once at the defined point. **Exceptions and concurrency:** an `On1x` value error is not silently converted into a C++ exception or process signal. **Limits:** honor the existing source/depth/size limits and add explicit budgets for patterns, CFG expansion, suspensions, active channels, captures, command size, and scheduler resources.

### Common syntax and lexical extensions

Introduce tokens `::`, `<-`, `..`, `..=`, `?`, `|`, `&`, `|>`, `??` only where needed by this specification, and a backtick shell literal (single and triple). Scan longest tokens first (`...` before `..=`, `..`; `<=` before `<`; `<-` as one token). Keep current `#` line comments; optionally add `//` comments, preserving scanner parity. Existing `->` in function types is unchanged; expression-leading `->(...)` means a channel constructor only in expression grammar. Triple-backtick *inside On1x source* is a dedicated multiline shell literal, not Markdown; escape or delimit it properly in documentation. Keywords may be contextual if identifier compatibility requires it; do not accidentally reserve every English word globally.

---

## 1. Generalized ML-style `match` and structural patterns

### Syntax

```on1x
match foo with {
    1                    => println("one"),
    [h1 :: h2 :: tail]    => { fizz(h1); fizz(h2); rocky(tail); },
    (a, b, c)            => foobar(b),
    Ok(value) if value > 0 => println("positive result"),
    Err(err)             => errln(err),
    rest                 => println("unmatched")
};

let output = match calculate() with {
    Ok(n) if n >= 0 => n * 2,
    Ok(_)           => 0,
    Err(_)          => -1
};

match let foo = bar() with {
    Ok(_)   => println("ok!"),
    Err(e)  => errln(e)
};
```

`match` is both a statement and an expression. Statements allow expression arms or statement blocks; expression arms must all have a unifiable result type. A block that is the arm of a **match expression** yields its final *unterminated* expression; a block ending in a semicolon has `void` type. To avoid ambiguity, bare expression statements elsewhere retain the ordinary mandatory semicolon. The scrutinee of `match let name = initializer with ...` evaluates the initializer exactly once, introduces an immutable binding scoped to all arms, and matches that bound value. It is a **restricted binding discriminant**, not permission to execute arbitrary statements as expressions.

`rest` is a reserved **catch-all pattern** with no binding, allowed only in the final arm. `_` is a wildcard that binds nothing. An ordinary identifier (such as `k`) is a *binder* and therefore also catches any remaining value; earlier catch-all binders make subsequent arms unreachable. The original draft's early `k => ...` must therefore move to the end or have a guard that permits later matches. Reject duplicate or unreachable arms where provable; reject non-exhaustive matches for closed sums and booleans, and require a wildcard/final binder for open/infinite domains unless a finite exhaustiveness proof exists.

### Pattern grammar and matching

```ebnf
match_expr      = 'match' match_discriminant 'with' '{' match_arms '}' ;
match_stmt      = match_expr [';'] ;
match_discriminant = expression | 'let' identifier '=' expression ;
match_arms      = match_arm { ',' match_arm } [','] ;
match_arm       = pattern ['if' expression] '=>' (expression | match_arm_block) ;
pattern         = '_' | 'rest' | literal | binder | constructor_pattern
                | tuple_pattern | list_pattern | array_pattern
                | record_pattern | pattern '|' pattern | pattern 'as' binder ;
constructor_pattern = identifier '(' [pattern {',' pattern}] ')' ;
tuple_pattern   = '(' pattern ',' pattern {',' pattern} ')' ;
list_pattern    = '[]' | '[' pattern {'::' pattern} ']' | '[' pattern {',' pattern} ']' ;
record_pattern  = 'record' '{' identifier ':' pattern
                  {',' identifier ':' pattern} [',' '..'] '}' ;
```

`[h :: t]` matches nonempty lists and binds `h` to an element and `t` to a list tail. `[h1 :: h2 :: t]` requires at least two elements. `[a,b]` matches exactly two elements; `[]` matches empty. For fixed arrays, support `[a,b]` only when the static extent is exactly two; array *tail slicing* must not masquerade as a list tail. Introduce a real owning `list<T>` runtime type before admitting list-cons patterns or comprehensions over lists. The `record{field: binder, ..}` pattern permits omitted fields. `p1 | p2` is legal only when both sides bind the same names with identical types, and it backtracks **without side effects**. Guards run after a successful structural test and binder initialization; guards must return `bool`. Match failure or an erroneous guard never leaks partially bound variables.

### Canonicalization and checking

Evaluate the scrutinee to a temp; compile a *pattern matrix* to a decision DAG (constructor tests, integer/boolean equality, tuple projections, array/list length and head/tail tests); bind arm-local variables; evaluate guards only after structural success; branch to one arm and one join block. Preserve top-to-bottom arm priority and source-level guards, avoid repeated destructuring work, and maintain provenance for diagnostics. Expression matches merge one typed result (SSA phi or result slot); statement matches merge control flow only. Never hand `Match` to a code generator.

Integrate the existing closed-sum `match(value){...}` as a backwards-compatible spelling in the new front-end. Validate nested, linear/nonlinear binder behavior: a name may bind at most once in one pattern; equality patterns require explicit syntax, not duplicate binders. Add satisfiability-assisted redundancy checks only when sound; a solver's `unknown` is not proof of exhaustiveness.

---

## 2. Hoare's CSP: internal/external fibers, channels, and `poll`

### A. Fibers

````on1x
let worker: ifiber<i64> = launch {
    var sum = 0;
    for x in 0..10 { sum = sum + x; }
    sum
};
let answer = join worker;     # wait synchronously, obtain worker result

let job: xfiber = launch `export K=12; ls $K`;
let batch: xfiber = launch ```
export K=12;
ls $K;
```;
let exit = join job;
````

An **internal fiber** executes On1x code; type `ifiber<T>` is a completion handle with a result. A **shell/external fiber** executes the portable On1x command grammar; type `xfiber` is a handle to an `exit_status` record plus bounded, separately consumable stdout/stderr streams. If the existing type system cannot infer block result values, use a trailing expression or explicit `return` *with a well-defined fiber-return context*; choose one rule consistently. `launch` is eager: it schedules the work immediately. Handles are linear or affine: dropping an unjoined handle is either prohibited or performs specified cancellation + join inside a task scope. Never silently detach a process.

**External shell grammar is not an arbitrary host shell.** Lex shell literals in a separate mode (single/double quotes, escapes, `$NAME`/`${NAME}` references, assignment, `export`, command invocation, `;`, `&&`, `||`, pipes, file redirection, a portable builtin set). Parse into a command AST using a MuLIB parser if one is suitable. Execute with explicit argv/env and platform-neutral spawn/pipeline/pipe adapters. On Unix use `posix_spawn` or a carefully checked `fork`/`exec` adapter; Windows uses native process APIs and its own argv quoting. Do not invoke `/bin/sh -c` or `cmd.exe /c` for the portable profile. Expansions are *not* recursively re-tokenized as shell code. Cap command/argv/environment sizes; define capture limits, exit status, process-group cancellation, timeouts, and cleanup. The shell **syntax** is portable, but arbitrary external executable names are not: `ls` will work cross-platform only if On1x supplies a portable built-in/compatibility executable or the user installs it on that platform; otherwise return `CommandNotFound`. Features requiring host-specific shells must be explicitly gated as nonportable.

The current runtime's closure/reference-count implementation is **single-threaded**. Until Chisel/Plaid admission and thread-safe ownership are implemented, `launch { ... }` may use **cooperative single-threaded fibers** only, and must reject unsafe mutable captures and resource-bearing data. Moving to native threads requires explicit `Send`/`Sync`-like capability checks or verified ownership transfer; using `pthread_create` alone is not sufficient.

### B. Channels

Preserve the draft's `->(1, 1) { ... }` spelling and give its integers unambiguous meaning: **maximum sender and receiver endpoint cardinalities**, respectively. They must be compile-time **positive** integers in the initial profile; **capacity is separate**. The constructor returns a channel descriptor that can be split into endpoints, with mandatory cleanup and close semantics.

```on1x
let channel = ->(1, 1) {
    payload: i64,
    capacity: 8,
    mode: uni
};
let (tx, rx) = split channel;

tx <- 42;                   # send, may suspend on backpressure
let value = <-rx;           # receive, may suspend until data or close
close tx;                   # end-of-stream after buffered messages drain

# Duplex gives both peers send and receive capabilities.
let pipe = ->(1, 1) {
    request: string,
    response: string,
    capacity: 0,
    mode: duplex
};
let (client, server) = split pipe;
client <- "hello";
let request = <-server;
server <- "world";
let response = <-client;
```

A unidirectional channel yields `sender<T>` and `receiver<T>`. A duplex channel yields paired `duplex<Outbound,Inbound>` / `duplex<Inbound,Outbound>` endpoints. `channel<T>` denotes an *unsplit channel descriptor*; split is a compiler-recognized consuming operation and is not a shallow copy. Static cardinalities are capability quotas checked at endpoint creation; require both counts to be positive in the initial channel profile and reject zero endpoints. There is no runtime reconfiguration of quotas. `capacity: 0` is a synchronous rendezvous requiring acknowledgment; positive capacity is bounded FIFO. Multi-sender ordering is FIFO **per sender**; no deterministic global interleaving guarantee. `close` is idempotence-checked; sending after close is a typed `channel_closed` outcome (or an explicit fault only in designated unchecked operations). Receiving on a drained, closed channel returns `Option<T>` (or a `Result<Option<T>, ChannelError>` if cancellation/errors are possible); examples that destructure `let value = <-rx` must handle that result rather than assuming a value. A `poll` receive binder likewise receives a *value only on a successful message arm* and must have a separate closure branch or a typed `Option` binding.

For ergonomics, prefer explicit checked variants:

```on1x
match <-rx with {
    Some(item) => println(item),
    None(_)    => println("closed")
};
```

A send **expression** returns `Result<void, ChannelError>` and must be handled or explicitly discarded. The statement syntax `tx <- payload;` propagates a checked `ChannelError` effect to the enclosing function/fiber; it is a compile-time error unless that effect is declared/inferred or handled. No silent send failure. Define fair scheduling, cancellation on send/receive, FIFO for buffered channels, rendezvous ownership transfer, and producer/consumer wakeup at close. For shared immutable/refcounted messages, provide correct atomics and lifetime handling; for owned messages, transfer a single owner. External process streams are **byte channels**, not automatically decoded UTF-8.

### C. `poll`: CSP choice / Go-like select

```on1x
poll {
    recv rx as packet => handle(packet),
    send tx <- make_reply() => println("sent"),
    recv control as command => handle_command(command),
    closed rx => println("input closed"),
    after 250ms => println("timeout"),
    default => println("nothing ready")
};

let response = poll {
    recv inbox as message => message,
    after 2s => "timed out"
};
```

`poll` is both an expression and a statement. **All channel expressions and pending send values are evaluated once** when entering the `poll`; *no send takes effect* before an arm is atomically committed. The runtime registers an all-or-none choice ticket across offered operations. Exactly one ready operation commits; all losing offers are withdrawn before executing the chosen body. With `default`, never suspend; without it, suspend until a ready communication, closure event, cancellation, or deadline. `after` is one monotonic deadline evaluated once. Use round-robin/randomized tie-breaking to avoid permanently favoring source order; define a seeded scheduler mode for deterministic tests. Support at most one `default` and a bounded number of arms. A `closed endpoint => arm` offer handles a drained/closed endpoint. An unhandled close becomes a typed channel error instead of being permanently ready with the wrong payload.

**Critical:** a `poll` implemented by testing each channel then blocking on one is racy and incorrect. LibStone Plaid Conduit currently has bounded/unbuffered channels and a cancellable receive-only selection substrate, but it is not proof of full atomic multi-directional select. Extend or wrap it to support safe send/receive/deadline offers, multi-party cancellation cleanup, and verified lifetimes; otherwise compile-time-reject those `poll` shapes. Implement source-level `poll` as a checked choice-state machine in Core, not a raw backend keyword.

---

## 3. Endless loops, `for`, loop labels, and loop results

```on1x
loop {
    if done() { break; }
    work();
}

for item in collection { visit(item); }
for (var i = 0; i < 10; i = i + 1) { visit(i); }
for i in 0..10 { visit(i); }          # half-open range: 0 through 9
for i in 0..=10 { visit(i); }         # inclusive range

outer: loop {
    for x in numbers {
        if x < 0 { continue outer; }
        if x == 42 { break outer; }
    }
}

let first = loop {
    let n = next_value();
    if valid(n) { break with n; }
};
```

`loop` is endless unless exited. `for pattern in iterable` requires an iterator protocol that returns `Option<T>` and has defined drop behavior; range iteration checks bound/step overflow. The three-clause `for` has lexically scoped initialization; `continue` executes the step exactly once. Loops may have a statement label with `identifier ':'` immediately before `loop`/`while`/`for`. `break label;` targets that loop, and `continue label;` starts its next iteration. Labels are a separate lexical namespace and cannot cross function/fiber/generator boundaries.

`loop` may be an expression when all reachable value exits use `break with expr` or `break label with expr`, with common result type; a loop with a reachable bare `break` is `void`. `while` and `for` are statement-only in v1; a later language revision may add `else`-on-normal-exhaustion, but not by silently reinterpreting existing loops. Lower into explicit header/body/latch/exit CFG blocks, preheaders for range bounds, and tagged cleanup edges. Never translate `continue` in a three-clause `for` to a jump that accidentally skips the step.

---

## 4. Deterministic resource cleanup: `defer`, `using`, and `scope`

```on1x
fn read_config(path: string) -> Result<string, IOError> {
    using file = open_file(path)?;
    defer { log("closing config scope"); }
    return Ok(file.read_all()?);
}

{
    let socket = open_socket()?;
    defer close(socket);
    send_message(socket, "ping")?;
}
```

`defer expression;` and `defer { statements }` register a cleanup when control reaches the defer. Execute registered cleanups **LIFO** on normal scope exit, `return`, `break`, `continue`, recoverable error propagation, and cooperative cancellation. A `using binder = expression;` acquires an owning resource and schedules a verified `Drop` operation for the surrounding scope; it may not copy, escape, or borrow across suspension unless Chisel proves safety. A `defer` captures the designated values at registration time according to explicit borrow/move rules; it is not an arbitrary late-rebound closure. `defer` actions must not suspend in the initial profile (no `await`, blocking channel operation, or `launch`), and must not mask a pre-existing error without a policy. Terminal process traps and force-killed subprocesses do **not** guarantee cleanup.

Canonicalize `defer`/`using` into a cleanup stack with explicit exit edges (no emitted `Defer` node); merge normal and exceptional exits without double-dropping. Integrate with existing Chisel memory/resource checks. This is a language construct, not a macro or PPX.

---

## 5. Typed error propagation: `try`, `catch`, `finally`, `throw`, and `?`

```on1x
fn parse_and_save(text: string) -> Result<i64, ParseError> {
    let value = parse_i64(text)?;
    save(value)?;
    return Ok(value);
}

try {
    let n = parse_i64(input)?;
    use(n);
} catch ParseError(err) {
    errln(err.message);
} catch IOError(err) {
    errln(err.message);
} finally {
    log("operation completed");
}
```

`expr?` is a postfix propagation operator on `Result<T,E>` or explicitly admitted `Option<T>`; it returns the success payload or takes a typed early-exit edge. `throw value;` signals a typed language error into an enclosing `try` or an effect-bearing function result. The *initial* implementation should canonicalize throws/handlers to explicit `Result`-based error edges, preserving cleanup; **do not depend on C++ exception unwinding** in the C11 backend. `catch` clauses use the same pattern compiler as `match`, are tried in order, and are checked for exhaustiveness/compatibility where the error set is closed. `finally` runs exactly once, after ordinary scope cleanup as specified by the Core exit policy. Runtime traps (bounds, invalid arithmetic, OOM) remain distinct and are not catchable unless separately converted to values by an explicit checked operation. A function's error effect must be declared or inferred; exposing it via an untyped FFI is prohibited.

---

## 6. Native async functions and `await`

```on1x
async fn fetch_and_decode(url: string) -> Result<string, NetError> {
    let bytes = await socket_read(url);
    return decode(bytes);
}

async fn pipeline() -> i64 {
    let f = fetch_count();        # future<i64>, not yet run or scheduled
    let value = await f;
    return value + 1;
}
```

`async fn f(...) -> T` produces `future<T>` when called; creation of a future does **not** itself imply `launch`. `await` is allowed only inside async functions or an explicitly async entry point, and suspends the containing task until its operand completes. Do not mix eager `launch` handles with lazy futures implicitly. Lower async routines to resumable states, frame slots, suspension and cleanup points; use Plaid **Weaver** to compute values live across suspension and **Loom/Continuum** or a C-ABI-compatible equivalent to execute them. In the minimal C11 profile use an explicit tagged frame and resume function; C++20 coroutines are an optional backend, not a language requirement. `await` with non-Send captures is allowed only on a same-thread executor. Check cancellation and all frame-owned drops on every terminal edge.

---

## 7. Generators, `yield`, and lazy iteration

```on1x
gen fn count_up(to: i64) -> i64 {
    for n in 0..to { yield n; }
}

for value in count_up(10) {
    println(value);
}
```

`gen fn f(...) -> T` returns `stream<T>`; `yield e;` sends one `T` to the consumer and suspends, preserving all live locals. The body finishes with normal exhaustion (`None`) or a typed failure; `return` may terminate but must not return an element. Define **single-consumer** ownership and explicit stream close/drop; no implicit cloning. Forbid yielding a reference to a local whose lifetime is not extended across suspension. Use the same suspension-frame machinery as async but keep a distinct operation and type. Iterator adaptors live in the standard library; core syntax consists of `gen`, `yield`, and `for ... in ...`.

---

## 8. Structured concurrency, cancellation, and deadlines

```on1x
taskgroup within 5s {
    let a = launch { compute_a() };
    let b = launch { compute_b() };
    let ra = join a;
    let rb = join b;
    println(ra + rb);
}

let worker = launch { keep_working() };
cancel worker;
let outcome = join worker;
```

`taskgroup { ... }` owns all child fibers started lexically within it. On normal exit it waits for outstanding children; on error or cancellation it requests cancellation, drains/join-reaps children, and propagates a typed outcome. `within duration` sets a **monotonic** deadline inherited by children; this is an admission/scheduling deadline, not a guarantee that a noncooperative native function stops instantly. `cancel` requests cancellation and is idempotent; `join` observes completion and performs ownership cleanup. Detached work requires an explicitly unsafe/privileged API outside ordinary syntax. Never allow a child with captured stack variables to outlive its owning scope. Propagate cancellation through nested `poll`, channel waits, async frames and subprocess process groups; ensure join from the same execution worker cannot deadlock. Add a deterministic scheduler for exhaustive race and cancellation tests.

---

## 9. Synchronization: `atomic` data and `lock` regions

```on1x
let counter: atomic<i64> = atomic(0);
atomic_store(counter, atomic_load(counter) + 1);  # not itself an atomic increment
let old = fetch_add(counter, 1);                    # actual RMW operation

let mu: mutex = mutex();
let shared_state: shared<State> = share(initial_state());
lock mu {
    update(shared_state);
}
```

`atomic<T>` is a type constructor for supported trivially copyable scalars with typed memory-ordering operations. An `atomic` load followed by a store is **not** an atomic read-modify-write transaction. Provide `compare_exchange`, `fetch_add`, and typed memory orders; require appropriate target ABI or reject. `lock mutex_expression { ... }` is a compiler-checked scoped lock: acquisition before entry, guaranteed release on exits, and detection of suspension/blocking-in-lock hazards. `shared<T>` is an owning concurrency-safe handle admitted by Chisel, not a loophole around `var`/ownership rules. Use Plaid Sentinel to diagnose potential lock-order cycles and races, never as a substitute for synchronization. Avoid an unsupported magical `atomic { multiple_statements }` construct: it would misleadingly imply a hardware or transactional guarantee.

---

## 10. Comprehensions and filtered collection construction

```on1x
let squares: list<i64> = list[x * x for x in 0..10];
let even: list<i64> = list[x for x in numbers if x % 2 == 0];
let pairs: list<(i64,i64)> = list[(x,y) for x in xs for y in ys if x != y];
```

Comprehension clauses bind in left-to-right nested order; each `if` filter is evaluated after its preceding `for` bindings and must return `bool`. List comprehensions create owning dynamic `list<T>` values, subject to allocation/length budgets and defined failure behavior; do **not** pretend dynamic output has the existing `array<T,N>` fixed extent. A later `collect array<T,N> { ... }` form may require compile-time or runtime exact-extent verification. Lower comprehensions to nested canonical `for`, conditionals, and checked builder operations with deterministic left-to-right effects. This is syntax, not PPX.

---

## 11. Named `record`/`enum` declarations and recursive ADTs

```on1x
record Point { x: f64, y: f64 }
enum Option<T> { None, Some(T) }
enum Result<T,E> { Ok(T), Err(E) }

enum Tree<T> {
    Leaf(T),
    Node(box<Tree<T>>, box<Tree<T>>)
}

fn total(tree: Tree<i64>) -> i64 {
    return match tree with {
        Leaf(n) => n,
        Node(left, right) => total(*left) + total(*right)
    };
}
```

Named type declarations introduce nominal identities and optional parametric constructors, with explicit normalization to the existing structural record/sum machinery only where representation equivalence is valid. Recursive constructors require an indirection (`box<T>` or equivalent) to avoid infinite layout; the initial milestone should support **nonrecursive** named records/enums, then add boxed recursive ADTs after allocation, ownership and drop checking are ready. Constructor resolution participates in import namespaces; match exhaustiveness uses the declared constructor catalog. Do not silently treat two distinct nominal types with identical fields as interchangeable. Record declaration syntax is distinguished from existing `record{...}` expressions by a following identifier.

---

## 12. Native function contracts and loop invariants

```on1x
fn divide(a: i64, b: i64) -> i64
    requires b != 0
    ensures result * b == a
{
    return a / b;
}

for i in 0..n invariant i >= 0 {
    process(i);
}
```

`requires` and `ensures` are part of the parsed function declaration; they are not decorators or PPX. `requires` is evaluated at entry, `ensures` on **normal successful return** with an immutable `result` binding, and `invariant` at loop entry and backedges as defined by the contract mode. Contract expressions are **pure** (no I/O, mutation, spawning, blocking, or unchecked FFI); `old(expr)` captures a pure, bounded entry-state value for postconditions. A verifier can use LibStone Nerotypical/Wetstone/Satie to prove a property; `unknown` must never count as proved. Default to checked runtime assertions for obligations not statically discharged, with provenance and deterministic failure diagnostics. An explicitly strict static mode rejects unproved obligations; an explicit trusted mode, if ever added, must be audited rather than silently turned on. Contracts must not change C ABI signatures or be elided without an explicit compiler policy.

---

## 13. Schematic source AST / ASDL extension

The current `On1x.asdl` is **only a token arena**. The following is a **design inventory** for a new source AST schema, *not* ready-to-run `asdl.pl` input. Write real generator-compatible syntax only after reading `third_party/libstone/scripts/asdl.pl` and existing ASDL examples. Keep one canonical owner for source locations and use generated structs or C++ bridge classes consistently.

```text
SurfaceExpr = Match(scrutinee, arms, expression_mode)
            | LaunchInternal(block) | LaunchExternal(shell_ast)
            | ChannelCreate(senders, receivers, channel_spec)
            | ChannelReceive(endpoint) | ChannelSend(endpoint, payload)
            | Poll(offers, expression_mode)
            | LoopExpr(label?, body) | TryExpr(body, catches, finally?)
            | Await(future) | Comprehension(element, clauses)
            | Propagate(value) | AtomicOp(op, args)
SurfaceStmt = MatchStmt(scrutinee, arms)
            | Loop(label?, body) | ForEach(label?, pattern, source, body)
            | ForC(label?, initializer, condition, step, body)
            | Break(label?, value?) | Continue(label?)
            | Defer(action) | Using(pattern, initializer)
            | TryStmt(body, catches, finally?) | Throw(error)
            | Yield(value) | TaskGroup(deadline?, body)
            | Cancel(handle) | Lock(mutex, body)
SurfacePattern = Wildcard | Rest | Bind(name) | Literal(value)
               | Variant(name, subpatterns) | Tuple(subpatterns)
               | ListNil | ListCons(head, tail) | ExactList(elements)
               | Array(elements) | Record(fields, has_rest)
               | Or(left, right) | As(pattern, name)
SurfaceDecl = RecordType(name, params, fields)
            | EnumType(name, params, variants)
            | Function(name, params, result, async?, gen?, contracts, body)
Offer       = Recv(endpoint, binder, arm) | Send(endpoint, payload, arm)
            | After(duration, arm) | Default(arm)
Contracts   = Requires(predicates) + Ensures(predicates) + Invariants(predicates)
```

Keep shell lexemes as separate scoped tokens/AST: `Word`, `Literal`, `VarExpand`, `Assignment`, `Command`, `Pipeline`, `Sequence`, `AndThen`, `OrElse`, `Redirect`. A validated command AST must be passed to the platform spawn adapter—never arbitrary synthesized host-shell source.

### Canonical Core forms (illustrative)

```text
Core = Block(params, instructions, terminator, cleanup_region, source_map)
Instructions: Const, Copy, Project, CheckTag, CheckLength, Compare, Call,
              Acquire, Release, Move, Drop, RegisterCleanup, CheckContract,
              ChannelCreate, ChannelSend, ChannelRecv, PollOffer,
              SpawnFiber, SpawnProcess, Await, Yield, RequestCancel,
              MutexAcquire, MutexRelease, AtomicRMW
Terminators: Jump, Branch, SwitchTag, Return, ErrorExit,
             Suspend(resume_state), TailCall, Unreachable
```

Purely syntactic constructs `Match`, `For`, `Defer`, `Using`, `Try`, and `Comprehension` must disappear before Core verification; semantic effects such as `SpawnFiber`, `ChannelSend`, `Await`, and `AtomicRMW` remain typed Core operations with explicit runtime mappings. Canonicalization must not erase concurrency effects. Maintain effect tokens or another sequencing discipline and refuse to merge/reorder effectful operations unsafely.

---

## 14. Implementation map for the existing repository

| Existing path | Required change |
|---|---|
| `on1x/grammar/On1x.g` | Backtick shell literal mode, new punctuation, keywords, scanner parity and token locations |
| `on1x/grammar/On1x.asdl` | Preserve token arena; add/version a source AST generator schema or bridge; regenerate outputs using LibStone scripts |
| `on1x/src/lexer_fallback.cpp`, `lexer.cpp` | Full lexical equivalence including malformed multiline shell literals |
| `on1x/src/parser.cpp` | Source productions, parse bounds, precedence, statement disambiguation, patterns and arm blocks |
| `on1x/include/on1x/ast.hpp` | Typed source AST nodes, source spans, pattern variants, effect/capability annotations |
| `on1x/src/infer.cpp`, `type.cpp`, type headers | Fiber/channel/future/stream/list/result/atomic/lock type constructors, pattern unification, effect inference |
| `on1x/src/sema.cpp`, `type_safety.cpp`, `safety.cpp` | Exhaustiveness, definite assignment, label targets, capture/ownership rules, lock/suspension checks, errors |
| `on1x/src/macros.cpp`, `ppx_ast.cpp`, `ppx_schema.cpp`, `grammar/PPX.*` | Preserve/core-lower new forms across hygienic expansion and versioned PPX boundaries without trust escalation |
| **New** `on1x/src/canonical/` + `include/on1x/canonical/` | Typed decision DAGs, CFG, cleanup, error exits, suspension-frame transformations, verification |
| `on1x/src/codegen.cpp` | Emit checked Core through C11 translation and explicit adapter APIs; no raw high-level match/poll |
| `on1x/src/backend/`, `include/on1x/backend/` | Extend MIR versions only with full verifier/interpreter/serializer/emitter compatibility; reject unsupported profiles clearly |
| `on1x/runtime/on1x_runtime.*` | Cooperative fiber scheduler, channel descriptors, shared ownership, cancellation, portable process adapter |
| `on1x/third_party/libstone` (submodule) | Integrate Plaid Conduit/Loom/Continuum/Weaver/Sentinel via checked C ABI; do not pretend C++20 library is native C11 |
| `include/mulib-*.h` | Reuse existing suitable portable parser/process/thread/IPC kernels through a single controlled implementation TU; add missing kernels only as header-only C11 and test separately |
| `on1x/tests/`, `on1x/examples/`, `docs/` | Golden parse tests, semantic negative tests, canonical IR snapshots, differential backend tests and docs |

The `on1x/third_party/libstone` entry is a Git submodule at an explicitly pinned commit: inspect the **pinned checkout**, not only LibStone's moving `master`. Do not vendor a second LibStone copy or change the pin without separately testing compatibility. Plaid's published implementation is partial; specifically, its existing select substrate is receive-only, so a fully general CSP `poll` needs additional runtime work.

### Suggested delivery stages

1. **G1 – Grammar/API freeze:** add scanner cases, AST definitions, grammar tests, and language manual; preserve existing programs.
2. **G2 – Pure lowering:** generalized patterns/match, loops, labels, comprehension syntax; decision-DAG/CFG verifier; extend C11 execution and MIR only for supported types.
3. **G3 – Cleanup and outcomes:** `defer`, `using`, `Result`/`?`, typed `try`/`throw`/`finally`, Chisel admission and deterministic exit tests.
4. **G4 – Cooperative concurrency:** internal fibers, channel endpoints, bounded send/recv, `poll` receive/deadline, cancellation and taskgroups; run in a single-threaded executor first.
5. **G5 – Extended concurrency:** atomically committed send/recv `poll`, portable external shell fibers, true parallel threads with ownership proofs, mutex/atomic operations, process cancellation and stress tests.
6. **G6 – Suspension and richer types:** `async`/`await`, generators, dynamic lists, named/boxed recursive ADTs, contracts and solver-assisted validation.

Each stage should be independently buildable. A feature is **complete** only when both generated and fallback scans agree, legal and illegal programs have stable semantics, Core is verified, supported backends execute correctly, unsupported targets fail explicitly, and tests pass under debug/release and configured sanitizers.

---

## 15. Conformance and adversarial test obligations

At minimum implement and commit tests for:

- **Match:** empty and nested structures; arm precedence; wildcard/rest placement; duplicate binders; or-pattern binder equivalence; guard effects; scrutinee once; sum exhaustiveness; tuple/array/list length; result-type joins; match-expression block result; old `match(value){...}` compatibility.
- **Loops:** zero iterations, range overflow, scope of `for` induction variable, `continue` step, labeled nested break/continue, loop-expression value/void conflicts, unreachable exits, cleanup on jumps.
- **Channels:** capacity 0/1/N; send-after-close; close with pending senders/receivers; MPMC FIFO per sender; endpoint cardinality; cancellation during rendezvous; ownership after send failure; double split; lost wakeup stress.
- **`poll`:** simultaneous ready branches; fairness over many iterations; no duplicated effects; send value evaluated once; cancellation of losing offers; close semantics; timeout/default races; deadline monotonicity; observer/registration cleanup; random seeded replay.
- **Fibers:** join twice, handle drop, child escape, capture of mutable or borrowed state, worker self-join, cooperative cancellation, nested taskgroups, resource cleanup, scheduler exhaustion.
- **External process grammar:** quotes and escaped whitespace, variable expansion without resplitting, env scoping, pipeline exit status, pipe backpressure, stdout/stderr byte limits, malicious interpolation, process-group termination, Windows/Unix parity when supported.
- **Errors/resources:** nested defer LIFO, using ownership, return/error/cancel unwinding, `finally` exactly once, unsupported trap catching, double drop, borrow across `await`, error effect/Result type mismatch.
- **Async/generators:** interleaved suspension, frame spill accuracy, cancellation before/after yield, no dangling borrowed views, early stream drop, deterministic executor replay.
- **Synchronization:** mutex poisoned/failed acquisition policy, lock ordering, blocking inside critical sections, atomic compare-exchange success/failure orderings, data-race stress, `shared<T>` ownership and send/sync rejection.
- **Contracts/ADTs:** unproved obligation treatment, `old` purity, recursive layout rejection without boxing, generic substitution, nominal type non-equivalence, exhaustive constructor matching.
- **Integration:** parse → macro/PPX → check → canonicalize → verify → C11; IR serialization/version rejection; no C11/C++ native thread calls without an ABI adapter; no feature advertised as implemented solely because a syntax-only test passes.

**Required deliverables from an implementation agent:** updated generator input/checked generated outputs (as project policy requires), scanner/parser/AST/semantic/canonicalization/runtime/backend patches, examples, negative tests, conformance results, a capability-matrix update, migration notes, and a per-feature status table (`SYNTAX_ONLY`, `CHECKED`, `LOWERED`, `EXECUTABLE`, `VERIFIED`). Do **not** mark a partially wired feature as fully complete.

## References examined for this design

- [`on1x/grammar/On1x.asdl`](https://github.com/Chubek/mulib/blob/master/on1x/grammar/On1x.asdl) and [`On1x.g`](https://github.com/Chubek/mulib/blob/master/on1x/grammar/On1x.g)
- [`on1x/src/parser.cpp`](https://github.com/Chubek/mulib/blob/master/on1x/src/parser.cpp) and [`include/on1x/ast.hpp`](https://github.com/Chubek/mulib/blob/master/on1x/include/on1x/ast.hpp)
- [`on1x/docs/architecture.md`](https://github.com/Chubek/mulib/blob/master/on1x/docs/architecture.md), [`language.md`](https://github.com/Chubek/mulib/blob/master/on1x/docs/language.md), [`data-model.md`](https://github.com/Chubek/mulib/blob/master/on1x/docs/data-model.md), [`capability-matrix.md`](https://github.com/Chubek/mulib/blob/master/on1x/docs/capability-matrix.md)
- [LibStone Plaid README](https://github.com/Chubek/libstone/blob/master/moonstone/plaid/README.md)
