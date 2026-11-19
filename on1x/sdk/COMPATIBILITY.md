# SDK compatibility policy

## ABI v1

ABI-v1 providers remain a supported compatibility profile. The adapter accepts
the exact existing v1 descriptor layouts, status values, synchronous callbacks,
source buffers, serialized verified-MIR analyzers/backends, and registry
lifetime rules.

V1 remains intentionally limited:

- no host operation table;
- no dependency graph or capability grant;
- no structured AST/HIR/MIR traversal;
- no candidate publication;
- no shared-memory data plane;
- no compiler or runtime embedding session.

A v1 provider is not silently granted v2 capabilities. The host may run it as a
trusted linked provider or through the existing compatible isolated profile.

## Source compatibility

Existing v1 headers and include paths remain installable. New declarations are
additive or provided through new headers. `On1xSDK.h` remains an umbrella, while
each C header must compile independently as C11 and C++17. The SDK 2 package
still satisfies `find_package(On1xSDK 1 CONFIG REQUIRED)` for existing
consumers of 1.0.0, but is not an exact 1.0.0 match.

## Binary compatibility

The SDK records ABI major version in each descriptor. V1 and v2 are dispatched
to separate layout readers. Appending a field does not authorize a callee to read
it unless `struct_size` covers the complete field. No C++ layout is public.

## Schema compatibility

AST, HIR, MIR, plugin protocol, manifest, contract, catalogue, and runtime ABI
versions are negotiated separately. Decoders reject unknown majors, malformed
lengths, trailing data when canonical form forbids it, and resource exhaustion.
Migration is explicit and deterministic; it never drops unsupported semantics.

## Driver behavior

Existing compiler modes and default C11 behavior remain available. New SDK
extensions require explicit activation. A plugin cannot replace mandatory
checking, the default backend, or foreign admission merely by reusing an old
name.
