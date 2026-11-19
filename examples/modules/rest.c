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
#define MULIB_IMPL_JSON
#define MULIB_IMPL_CDD_REST
#include "detail/mulib-tls-implementation.h"
#include "mulib-rest.h"
#include <stdio.h>
#include <stdlib.h>

static int offline(void *user, const mulib_uri_t *uri, const void *wire, size_t n,
                   mulib_webclient_receive_fn receive, void *receiver) {
  static const char reply[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                             "Content-Length: 13\r\n\r\n{\"name\":\"Go\"}";
  (void)user; (void)uri; (void)wire; (void)n;
  return receive(receiver, reply, sizeof reply - 1);
}

int main(void) {
  mulib_muw3_client_t client = mulib_muw3_client_default();
  const mulib_http_header_t pair = {"name", "Go"};
  char *url = mulib_cdd_rest_url_query("http://example.test/languages", &pair, 1, 0);
  mulib_cdd_rest_response_t response = {0};
  int rc;
  if (!url) return 1;
  client.http = offline;
  rc = mulib_cdd_rest_request(&client, "GET", url, NULL, 0, NULL, 1, &response);
  free(url);
  if (rc != 1) return 1;
  printf("HTTP %u\n", response.http.message.status);
  mulib_cdd_rest_response_free(&response);
  return 0;
}
