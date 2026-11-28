# WebIDL binding integration status

This is the P0 WebIDL portion of the browser additions review ledger. The
[compiler's MicroJS backend](mulib-webidl.md#microjs-native-bindings) can emit
and execute strict primitive operation thunks in the current VM, without
changing the compiler's parser dependency graph, adding a runtime dependency
to the compiler, or using the staging helpers. Generated consumers use
MicroJS/MiniVM and the existing WebIDL runtime/Bounded Text/UTF-8 modules.
The legacy [bindgen](mulib-webidl-bindgen.md) and [jsdom](mulib-jsdom.md) APIs
are unchanged. This is trusted-host integration, not browser conformance or
permission to expose the VM to untrusted scripts.

| Staging row | Executable slice and remaining work |
| --- | --- |
| `webidl-convert` | Generated thunks use the distinct upstream runtime's strict integer checks, with tagged boolean and finite-double handling. No ECMAScript coercion, modulo/clamp semantics, or string conversion; the staging helper is unchanged. |
| `webidl-overload` | One fixed arity per operation. Overloaded names, optional arguments, and variadics are rejected, not resolved. |
| `webidl-dictionary` | Not lowered. Needs property `Get`, inheritance, ordered conversion, defaults and exception propagation. |
| `webidl-union` | Not lowered. Needs WebIDL distinguishability, dispatch precedence and nullable/dictionary rules. |
| `webidl-callback` | Not lowered. The native thunk is not a WebIDL callback. Needs callable validation, lifetime/rooting, relevant realm, and an explicit way to invoke script from native code. |
| `webidl-platformobject` | No objects or receivers are installed. Needs native storage/brand checks, receiver-aware native methods, prototypes and descriptors. |
| `webidl-exposure` | Extended attributes are rejected. The host opts in to installation per VM; this is not `[Exposed]`, `[SecureContext]`, or feature policy enforcement. |
| `webidl-exception` | Arity/type errors return `MULIB_MICROJS_TYPE`; no exception values or DOMException translation. |
| `webidl-wrappercache` | No wrapper objects or cache. Needs realm-local identity and GC/lifetime rules rather than unowned pointer keys alone. |
| `webidl-install` | One generated installer per operation calls MicroJS global binding directly. No namespace object, prototypes, exposure plan, or transactional multi-member bootstrap. |

## Direction for native DOM bindings

The current [MicroJS](mulib-microjs.md) public API can install a native global
and manipulate ordinary own data properties. The native callback has no
receiver argument, `bind` publishes a global, and the public API has no
descriptor/accessor or host-object tracing/finalization hooks. Copying a
global function onto an ordinary object does not supply those semantics. A
staged object descriptor or stringified `jsdom` operation should not be used
as a second VM object system.

1. Add VM-owned native functions with receiver-aware calls and a way to define
   data/accessor properties and prototype links without publishing temporary
   globals. Define native exception values and reentrancy rules before
   binding synchronous DOM event listeners. Existing native callbacks must
   not reenter evaluation, calls or collection.
2. Attach an unforgeable native brand and lifetime record to VM objects.
   Keep one native [DOM](mulib-dom.md) tree; wrappers must refer to live
   document-owned nodes, not a parallel tree or script-supplied integer
   identities. Document destruction invalidates nodes, and adoption can
   transfer their ownership to a different document. Define a revocable or
   retaining bridge contract covering both transitions before exposing wrappers.
   Define per-VM identity/weak cache behavior with tracing and teardown;
   retaining a public rooted result indefinitely is not weak caching.
3. As a first DOM slice, map a reviewed Node/Document subset to explicit
   native adapters. Add a length-delimited WTF-8 ↔ UTF-16 bridge that preserves
   lone surrogates and embedded NUL for `DOMString`; strict UTF-8 validation is
   not a substitute. Distinguish null from empty strings, map native errors,
   and test invalid receivers, detach/adopt, VM collection and document teardown
   before expanding to accessors, callbacks and IDL corpus lowering.
