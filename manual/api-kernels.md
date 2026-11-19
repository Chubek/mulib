# MuLIB HTTP and wire protocol kernels

| Module | Scope |
| --- | --- |
| [REST](mulib-rest.md) | HTTP transactions, status-aware JSON, and query parameters |
| [GraphQL](mulib-graphql.md) | GraphQL-over-HTTP JSON documents and error reporting |
| [JSON-RPC 2.0](mulib-jsonrpc.md) | Request/response envelopes, notifications, batches and correlation |
| [SOAP](mulib-soap.md) | SOAP versions 1.1/1.2 XML envelopes and HTTP exchange |
| [gRPC](mulib-grpc.md) | Length-prefixed frames and binary unary gRPC-Web |
| [SSE client](mulib-sse-client.md) | One HTTP stream delivered to an incremental event callback |
| [WebSocket](mulib-websocket.md) | Opening handshake and frame processing for caller-owned connections |

These modules extend the supplied `protcol-kern` headers in the main MuLIB
tree. They do not use a new network library: the HTTP clients build on
[MuW3](mulib-muw3.md), and the codecs use MuLIB JSON, XML, buffer, and SSE
primitives as applicable. Each module's manual lists its exact scope.

Define the `MULIB_IMPL_CDD_*` switch in exactly one C translation unit per
module. The original `MULIB_IMPL_*` switches remain compatibility aliases.
Do not rely on a module enabling its dependencies: use the example or the
dependency closure in the manifest. C++17 consumers can include declarations
and link a separately compiled C implementation.

## Boundaries

- Keep request inputs and callback configuration alive for the synchronous
  call and free outputs with the corresponding destructor. Transport success
  is not an application success; check HTTP, JSON-RPC, GraphQL, SOAP or gRPC
  status at the appropriate layer.
- Use verified HTTPS for remote peers, provide bounded timeouts and body
  limits, and scope credentials to the intended server. Callers providing a
  custom transport must enforce those properties themselves.
- These are codecs and clients, not services. They do not interpret GraphQL
  programs, implement native HTTP/2 gRPC, provision endpoints, or manage
  background connections. SOAP fragments are trusted markup, not a place to
  interpolate user-provided XML. A WebSocket codec does not itself open a
  socket or authenticate a peer.

## Verification

`make check-api-kernels` runs the focused C tests, then compiles every
implementation independently with both switch spellings and links a C++17
consumer against actual MuLIB kernels. The CTest suite also includes native
TCP loopback exchanges for REST, GraphQL, JSON-RPC, gRPC-Web, SOAP, and SSE
against an independent Python wire fixture. The imported
`tests/api-kernels/run.sh` uses substitute dependencies and takes the
original archive's `include` directory as its argument; it checks that
baseline only, not these MuLIB kernels.

With Clang, `-DMULIB_BUILD_FUZZER=ON` adds `mulib_fuzz_api_kernels`.
Successful frame parses must re-encode to frames with identical opcode,
flags, lengths and decoded binary payloads.
