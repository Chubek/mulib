/* Structural and binary round-trip properties for the two frame codecs. */
#include "mulib-grpc.h"
#include "mulib-websocket.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *input, size_t length)
{
  const unsigned char mask[4] = {1, 2, 3, 4}; /* deterministic fixture only */
  mulib_buffer_t wire = {0};
  mulib_websocket_frame_t ws = {0}, decoded = {0};
  mulib_grpc_frame_t grpc = {0}, other = {0};
  size_t used = 0, encoded = 0;
  if (length > 8192) return 0;
  if (mulib_websocket_frame_read(input,length,4096,-1,&ws,&used) == 1) {
    assert(used >= 2 && used <= length && ws.length <= 4096);
    assert(mulib_websocket_frame_write(&wire,ws.opcode,(int)ws.fin,
                                      (int)ws.masked,mask,ws.payload,ws.length));
    assert(mulib_websocket_frame_read(wire.data,wire.length,4096,
                                    (int)ws.masked,&decoded,&encoded) == 1);
    assert(encoded == wire.length && ws.length == decoded.length &&
           ws.opcode == decoded.opcode && ws.fin == decoded.fin);
    assert(!ws.length || !memcmp(ws.payload,decoded.payload,ws.length));
    mulib_buffer_free(&wire);
  }
  mulib_websocket_frame_free(&decoded);
  mulib_websocket_frame_free(&ws);
  if (mulib_grpc_frame_read(input,length,4096,&grpc,&used) == 1) {
    assert(used >= 5 && used <= length && grpc.length <= 4096);
    assert(mulib_grpc_frame_write(&wire,grpc.compressed,grpc.data,grpc.length));
    assert(mulib_grpc_frame_read(wire.data,wire.length,4096,&other,&encoded) == 1);
    assert(encoded == wire.length && grpc.length == other.length &&
           grpc.compressed == other.compressed);
    assert(!grpc.length || !memcmp(grpc.data,other.data,grpc.length));
    mulib_buffer_free(&wire);
  }
  return 0;
}
