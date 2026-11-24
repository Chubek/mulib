# Mune — discovery-only: `gc_roots_and_callback_authority`

Unresolved semantics from `kernels/mulib-mune.h`, `manual/mulib-mune.md`,
`on1x/contracts-exoml/mune.exocontract.json` (52 entries) and the Mune
section of `on1x/contracts-exoml/obligations.json`. The Exotract Mune example
is discovery evidence, not prior safe approval. No legacy Gate B or native
archive result transfers trust into FFIHandler.

- Legacy numeric environment, reference-counted chunk, runtime state and result
  roots are distinct owners. Chunk AST is borrowed; a closure can retain
  a chunk after caller destruction. Result destruction unroots values, not
  directly destroys the GC objects; drop results before the state. Successful
  run/call publishes an owned result root through an initially NULL slot;
  callback args/state/output builders are borrowed. Conditional output
  initialization and allocator/finalizer/resurrection paths remain unresolved.
- The native tagged enum/union aggregate and nested objects require exact ABI
  and same-state GC identity. Copying a tagged value, string/userdata pointer,
  or upvalue identity does not root it: use a rooted global/table/result as
  specified. Constructors return state borrows, not independent malloc
  ownership. Userdata bytes are state-owned and zeroed; value/userdata length
  is conditional. Runtime strings are binary, not On1x strings. Exact `size_t`,
  i64, double, enum/tag and input extent handling are separate obligations.
- Do not mutate mode or destroy an executing state; serialize state/chunk/
  legacy environment. `bind`/`make_function` retain callback/user referents,
  not their ownership, conservatively through whole-state destruction;
  `function_owner` can root backing userdata. **No** per-function revoke.
  Calls/metamethods may reenter arbitrary native/host code; native callbacks
  may run on a coroutine's C11 worker. No same-thread IPC adapter reuse.
  Writer callbacks synchronously borrow their context; AOT entries,
  names and code must outlive all closures retaining the chunk.
- Numeric eval commits environment/output only on success. Runtime failures
  preserve earlier effects; early invalid arguments may leave diagnostics
  untouched. Parse copies source and owns the checked AST; parse diagnostic
  strings are static, execution diagnostics can borrow the state until next
  execution. Raw table/global operations and iteration do not invoke Lua;
  metamethod-aware accesses do, and root their operands. Failed getters
  preserve output. Iteration end may change its cursor without a value result.
- Bundle load verifies images before registration and does not execute them;
  registration may partially commit on failure. Writer errors may leave
  partially generated C/bundle output. Existing package/searcher registry,
  dump/load and PRNG/native-module behavior are not ABI or authority proof.
  Full Mune explicitly enables the optional MANIFEST parser implementation
  in the dependency closure; standalone Parsec does not.
- Execution is not a sandbox. Lua libraries, finalizers, native modules and
  callbacks can do I/O, change locale, suspend or terminate the host; JIT
  depends on the host. Explicit native authority and synchronization are
  unresolved; no safe or unsafe generic call profile is admitted here.
