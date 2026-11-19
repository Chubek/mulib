#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_TSL
#define MULIB_IMPL_HTTPS
#define MULIB_IMPL_SOCKET
#define MULIB_IMPL_MUW3
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SSE
#define MULIB_IMPL_CDD_SSE_CLIENT
#include "detail/mulib-tls-implementation.h"
#include "mulib-sse-client.h"
#include <stdio.h>

/* Local transport fixture: this example makes no network requests. */
static int fixture(void *user, const mulib_uri_t *endpoint, const void *request,
    size_t length, mulib_webclient_receive_fn receive, void *receiver) {
  static const char reply[] = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
      "Content-Length: 13\r\n\r\ndata: hello\n\n";
  (void)user;
  (void)endpoint;
  (void)request;
  (void)length;
  return receive(receiver, reply, sizeof reply - 1);
}

static int print_event(void *user, const mulib_sse_event_t *event) {
  size_t *count = (size_t *)user;
  ++*count;
  printf("%s: ", event->event);
  if (fwrite(event->data, 1, event->data_length, stdout) != event->data_length)
    return 0;
  return putchar('\n') != EOF;
}

int main(void) {
  mulib_muw3_client_t client = mulib_muw3_client_default();
  mulib_muw3_response_t response = {0};
  size_t count = 0;
  int rc, ok;
  client.http = fixture;
  rc = mulib_cdd_sse_client_get(&client, "http://example.test/events", NULL,
      print_event, &count, &response);
  ok = rc == 1 && response.message.status == 200 && count == 1;
  mulib_muw3_response_free(&response);
  return ok ? 0 : 1;
}
