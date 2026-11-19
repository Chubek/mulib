/* One TU for the canonical switches in the loopback integration build. */
#define MULIB_IMPL_CDD_REST
#define MULIB_IMPL_CDD_GRAPHQL
#define MULIB_IMPL_CDD_JSONRPC
#define MULIB_IMPL_CDD_GRPC
#define MULIB_IMPL_CDD_SOAP
#define MULIB_IMPL_CDD_SSE_CLIENT
#define MULIB_IMPL_CDD_WEBSOCKET
#include "mulib-rest.h"
#include "mulib-graphql.h"
#include "mulib-jsonrpc.h"
#include "mulib-grpc.h"
#include "mulib-soap.h"
#include "mulib-sse-client.h"
#include "mulib-websocket.h"
