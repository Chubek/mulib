#define MULIB_IMPL_BASE64
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_CDD_WEBSOCKET
#include "mulib-websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  static const char reply[] = "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n\x81\x05" "Hello";
  /* RFC 6455 fixture input only. Use a fresh CSPRNG key/mask in a real client. */
  char key[25], *request;
  mulib_http_message_t headers = {0};
  mulib_cdd_websocket_frame_t frame = {0};
  mulib_cdd_websocket_sequence_t state = {0};
  size_t used, frame_bytes;
  int ok = 0;
  if (!mulib_cdd_websocket_key((const unsigned char *)"the sample nonce", key))
    return 1;
  request = mulib_cdd_websocket_upgrade_request("example.test", "/chat", key, NULL);
  if (!request) return 1;
  free(request); /* A real client sends these bytes on its transport first. */
  if (mulib_http_parse(reply, sizeof reply - 1, 1, NULL, &headers, &used) == MULIB_HTTP_OK
      && mulib_cdd_websocket_validate_upgrade(&headers, key)
      && mulib_cdd_websocket_frame_read(reply + used, sizeof reply - 1 - used,
          1024, 0, &frame, &frame_bytes) == 1
      && mulib_cdd_websocket_sequence_check(&state, &frame, 1024) == 1
      && frame.opcode == MULIB_CDD_WEBSOCKET_TEXT
      && frame.length == 5 && !memcmp(frame.payload, "Hello", frame.length)) {
    ok = fwrite(frame.payload, 1, frame.length, stdout) == frame.length && putchar('\n') != EOF;
  }
  mulib_cdd_websocket_frame_free(&frame);
  mulib_http_message_free(&headers);
  return ok ? 0 : 1;
}
