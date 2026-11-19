#include "mulib-websocket.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char key[] = "dGhlIHNhbXBsZSBub25jZQ==";
static const char accept[] = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

static void handshake(void) {
  static const char reply[] = "HTTP/1.1 101 Switching Protocols\r\n"
      "UpGrade: WebSocket\r\nConnection: keep-alive\r\n"
      "connection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n\x81\0";
  mulib_http_message_t parsed = {0}, request = {0};
  mulib_http_header_t fields[] = {
    {"Upgrade", "websocket"}, {"Connection", ", keep-alive , upGRADE,"},
    {"Sec-WebSocket-Accept", (char *)accept}, {"Sec-WebSocket-Protocol", "chat"}
  };
  mulib_http_message_t direct = {.is_response = 1, .status = 101, .headers = fields, .header_count = 4};
  char out[29], *wire;
  size_t used;
  assert(mulib_cdd_websocket_key((const unsigned char *)"the sample nonce", out));
  assert(!strcmp(out, key));
  assert(mulib_cdd_websocket_accept(out, out) && !strcmp(out, accept));
  assert(!mulib_cdd_websocket_accept("dGhlIHNhbXBsZSBub25jZR==", out));
  assert(!mulib_cdd_websocket_accept("short", out) && !strcmp(out, accept));
  assert(mulib_http_parse(reply, sizeof reply - 1, 1, NULL, &parsed, &used) == MULIB_HTTP_OK);
  assert(used == sizeof reply - 3);
  assert(mulib_cdd_websocket_validate_upgrade(&parsed, key));
  mulib_http_message_free(&parsed);
  assert(!mulib_cdd_websocket_validate_upgrade(&direct, key));
  assert(mulib_websocket_validate_upgrade(&direct, key));
  assert(mulib_cdd_websocket_validate_upgrade_protocol(&direct, key, "chat"));
  assert(!mulib_cdd_websocket_validate_upgrade_protocol(&direct, key, "Chat"));
  assert(!mulib_cdd_websocket_validate_upgrade_protocol(&direct, key, "chat, other"));
  direct.header_count = 3;
  assert(mulib_cdd_websocket_validate_upgrade_protocol(&direct, key, "chat"));
  fields[0].value = "websocket junk";
  assert(!mulib_cdd_websocket_validate_upgrade(&direct, key));
  fields[0].value = "TLS/1.3, websocket";
  assert(!mulib_cdd_websocket_validate_upgrade(&direct, key));
  fields[0].value = "websocket";
  fields[1].value = "upgrade, keep-alive garbage";
  assert(!mulib_cdd_websocket_validate_upgrade(&direct, key));
  fields[1].value = "upgrade";
  direct.header_count = 4;
  fields[3].name = "sec-websocket-accept";
  fields[3].value = (char *)accept;
  assert(!mulib_cdd_websocket_validate_upgrade(&direct, key));
  fields[3].name = "Sec-WebSocket-Extensions";
  fields[3].value = "";
  assert(!mulib_cdd_websocket_validate_upgrade(&direct, key));
  fields[3].name = "Content-Length";
  fields[3].value = "0";
  assert(!mulib_cdd_websocket_validate_upgrade(&direct, key));
  assert(!mulib_cdd_websocket_upgrade_request("test\r\nBad: x", "/", key, NULL));
  assert(!mulib_cdd_websocket_upgrade_request("a@host", "/", key, NULL));
  assert(!mulib_cdd_websocket_upgrade_request("host:65536", "/", key, NULL));
  assert(!mulib_cdd_websocket_upgrade_request("host", "//authority/a", key, NULL));
  assert(!mulib_cdd_websocket_upgrade_request("host", "/a#b", key, NULL));
  assert(!mulib_cdd_websocket_upgrade_request("host", "/bad%q0", key, NULL));
  assert(!mulib_cdd_websocket_upgrade_request("host", "/", key, "\xc3\xa9"));
  wire = mulib_cdd_websocket_upgrade_request("[::1]:443", "/a'%2F?a=b/c", key, "Chat.v1");
  assert(wire && strstr(wire, "Sec-WebSocket-Protocol: Chat.v1\r\n"));
  assert(mulib_http_parse(wire, strlen(wire), 0, NULL, &request, NULL) == MULIB_HTTP_OK);
  assert(!strcmp(request.target, "/a'%2F?a=b/c"));
  mulib_http_message_free(&request);
  free(wire);
}

static void frames(void) {
  static const unsigned char malformed[][10] = {
    {0xc2, 0}, {0x83, 0}, {0x09, 0}, {0x8a, 126}, {0x88, 1},
    {0x82, 126, 0, 125}, {0x82, 127, 0, 0, 0, 0, 0, 0, 255, 255},
    {0x82, 127, 0x80}, {0x81, 2, 0xc0, 0x80}, {0x88, 2, 0x03, 0xed},
    {0x88, 3, 0x03, 0xe8, 0x80}, {0x88, 2, 0x03, 0xf2}
  };
  static const size_t lengths[] = {1, 1, 1, 2, 2, 4, 10, 3, 4, 4, 5, 4};
  static const unsigned char big[] = {0x82, 127, 0, 0, 0, 0, 0, 1, 0, 0};
  static const unsigned char over_default[] = {0x82, 127, 0, 0, 0, 0, 1, 0, 0, 1};
  static const unsigned char mask[] = {0x37, 0xfa, 0x21, 0x3d};
  static const unsigned char expected[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d,
      0x7f, 0x9f, 0x4d, 0x51, 0x58};
  static const unsigned char close1010[] = {0x03, 0xf2};
  unsigned char *payload = (unsigned char *)calloc(65536, 1);
  mulib_buffer_t b = {0};
  mulib_cdd_websocket_frame_t frame = {.opcode = 55};
  size_t used = 77;
  assert(payload);
  assert(mulib_cdd_websocket_frame_read(NULL, 0, 0, 1, &frame, &used) == 0);
  assert(mulib_cdd_websocket_frame_read(NULL, 1, 0, 1, &frame, &used) == -1);
  for (size_t i = 0; i < sizeof lengths / sizeof *lengths; ++i)
    assert(mulib_cdd_websocket_frame_read(malformed[i], lengths[i], 0, -1, &frame, &used) == -1);
  assert(mulib_cdd_websocket_frame_read(big, sizeof big, 65535, 0, &frame, &used) == -2);
  assert(mulib_cdd_websocket_frame_read(big, sizeof big, 65536, 0, &frame, &used) == 0);
  assert(mulib_cdd_websocket_frame_read(over_default, sizeof over_default, 0, 0, &frame, &used) == -2);
  assert(frame.opcode == 55 && used == 77);
  assert(mulib_cdd_websocket_frame_write(&b, 1, 1, 1, mask, "Hello", 5));
  assert(b.length == sizeof expected && !memcmp(b.data, expected, sizeof expected));
  for (size_t i = 0; i < b.length; ++i)
    assert(mulib_cdd_websocket_frame_read(b.data, i, 0, 1, &frame, &used) == 0);
  assert(mulib_cdd_websocket_frame_read(b.data, b.length, 0, 0, &frame, &used) == -1);
  assert(!mulib_cdd_websocket_frame_write(&b, 8, 1, 0, NULL, close1010, sizeof close1010));
  assert(!mulib_cdd_websocket_frame_write(&b, 8, 1, 1, mask, "x", 1));
  assert(!mulib_cdd_websocket_frame_write(&b, 9, 1, 0, NULL, payload, 126));
  assert(!mulib_cdd_websocket_frame_write(&b, 1, 1, 0, NULL, "\xc0\x80", 2));
  assert(frame.opcode == 55 && used == 77 && b.length == sizeof expected);
  assert(mulib_cdd_websocket_frame_write(&b, 8, 1, 1, mask, close1010, sizeof close1010));
  assert(mulib_cdd_websocket_frame_read(b.data, b.length, 0, 1, &frame, &used) == 1);
  assert(used == sizeof expected && frame.length == 5 && !memcmp(frame.payload, "Hello", 5));
  mulib_cdd_websocket_frame_free(&frame);
  assert(mulib_cdd_websocket_frame_read(b.data + used, b.length - used, 0, 1, &frame, NULL) == 1);
  assert(frame.opcode == 8 && !memcmp(frame.payload, close1010, sizeof close1010));
  mulib_cdd_websocket_frame_free(&frame);
  mulib_buffer_free(&b);
  {
    const size_t boundaries[] = {0, 125, 126, 65535, 65536};
    for (size_t i = 0; i < sizeof boundaries / sizeof *boundaries; ++i) {
      size_t len = boundaries[i], head = len < 126 ? 2 : len < 65536 ? 4 : 10;
      assert(mulib_cdd_websocket_frame_write(&b, 2, 1, 0, NULL, payload, len));
      assert(b.length == head + len);
      assert(mulib_cdd_websocket_frame_read(b.data, b.length, len, 0, &frame, &used) == 1);
      assert(used == b.length && frame.length == len);
      mulib_cdd_websocket_frame_free(&frame);
      b.length = 0;
    }
  }
  while (b.length < b.capacity) {
    size_t left = b.capacity - b.length;
    assert(mulib_buffer_append(&b, payload, left < 65536 ? left : 65536));
  }
  used = b.length;
  assert(mulib_cdd_websocket_frame_write(&b, 2, 1, 1, b.data, b.data, 125));
  assert(mulib_cdd_websocket_frame_read(b.data + used, b.length - used, 0, 1, &frame, NULL) == 1);
  assert(frame.length == 125 && !memcmp(frame.payload, payload, 125));
  mulib_cdd_websocket_frame_free(&frame);
  mulib_buffer_free(&b);
  free(payload);
}

static void sequences(void) {
  mulib_cdd_websocket_sequence_t state = {0};
  unsigned char first[] = {0xf0, 0x9f}, end[] = {0x8c, 0x8d}, bad[] = {0, 0};
  mulib_cdd_websocket_frame_t start = {1, 0, 0, first, sizeof first};
  mulib_cdd_websocket_frame_t tail = {0, 1, 0, end, sizeof end};
  mulib_cdd_websocket_frame_t ping = {9, 1, 0, NULL, 0};
  mulib_cdd_websocket_frame_t close = {8, 1, 0, NULL, 0};
  assert(mulib_cdd_websocket_sequence_check(&state, &tail, 4) == -1);
  assert(mulib_cdd_websocket_sequence_check(&state, &start, 4) == 1);
  assert(mulib_cdd_websocket_sequence_check(&state, &start, 4) == -1);
  assert(mulib_cdd_websocket_sequence_check(&state, &ping, 1) == 1);
  assert(mulib_cdd_websocket_sequence_check(&state, &tail, 3) == -2);
  tail.payload = bad;
  assert(mulib_cdd_websocket_sequence_check(&state, &tail, 4) == -1);
  tail.payload = end;
  tail.length = 1;
  assert(mulib_cdd_websocket_sequence_check(&state, &tail, 4) == -1);
  tail.length = 2;
  assert(mulib_cdd_websocket_sequence_check(&state, &tail, 4) == 1);
  assert(mulib_cdd_websocket_sequence_check(&state, &tail, 4) == -1);
  assert(mulib_cdd_websocket_sequence_check(&state, &start, 4) == 1);
  assert(mulib_cdd_websocket_sequence_check(&state, &close, 4) == 1);
  assert(mulib_cdd_websocket_sequence_check(&state, &tail, 4) == -1);
  assert(mulib_cdd_websocket_sequence_check(&state, &close, 4) == -1);
}

int main(void) {
  handshake();
  frames();
  sequences();
  return 0;
}
