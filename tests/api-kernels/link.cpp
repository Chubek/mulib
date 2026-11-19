/* Declaration-only C++ consumer of the seven C11 implementation switches. */
#include "mulib-graphql.h"
#include "mulib-grpc.h"
#include "mulib-jsonrpc.h"
#include "mulib-rest.h"
#include "mulib-soap.h"
#include "mulib-sse-client.h"
#include "mulib-websocket.h"
#include <cassert>
#include <cstdlib>
#include <cstring>

int main()
{
  mulib_buffer_t wire{};
  mulib_grpc_frame_t grpc{};
  mulib_websocket_frame_t ws{};
  mulib_rest_response_t rest{};
  mulib_graphql_result_t graphql{};
  mulib_jsonrpc_reply_t rpc{};
  mulib_muw3_response_t stream{};
  size_t consumed = 0;
  char accept[29];
  // No reliance on substitute JSON/HTTP/base64/buffer functions.
  mulib_json_t *call = mulib_jsonrpc_make("example.read", nullptr, nullptr);
  assert(call && !mulib_json_get(call, "id"));
  mulib_json_destroy(call);
  auto text = mulib_soap_escape_text("<&", 2, 32);
  assert(text && std::strcmp(text, "&lt;&amp;") == 0);
  std::free(text);
  assert(mulib_grpc_frame_write(&wire, 0, "\0x", 2));
  assert(mulib_grpc_frame_read(wire.data, wire.length, 2, &grpc, &consumed) == 1);
  assert(consumed == wire.length && grpc.length == 2 && !memcmp(grpc.data, "\0x", 2));
  mulib_buffer_free(&wire);
  assert(mulib_websocket_accept("dGhlIHNhbXBsZSBub25jZQ==", accept));
  assert(std::strcmp(accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);
  assert(mulib_websocket_frame_write(&wire, MULIB_WEBSOCKET_BINARY, 1, 0, nullptr, "x", 1));
  assert(mulib_websocket_frame_read(wire.data, wire.length, 1, 0, &ws, &consumed) == 1);
  assert(ws.length == 1 && ws.payload[0] == 'x');
  mulib_websocket_frame_free(&ws);
  mulib_buffer_free(&wire);
  assert(mulib_sse_client_get(nullptr, nullptr, nullptr, nullptr, nullptr, &stream) < 0);
  mulib_rest_response_free(&rest);
  mulib_graphql_result_free(&graphql);
  mulib_jsonrpc_reply_free(&rpc);
  mulib_muw3_response_free(&stream);
}
