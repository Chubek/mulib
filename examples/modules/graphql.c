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
#define MULIB_IMPL_CDD_GRAPHQL
#include "detail/mulib-tls-implementation.h"
#include "mulib-graphql.h"
#include <stdio.h>

static int offline(void *user, const mulib_uri_t *uri, const void *wire, size_t n,
                   mulib_webclient_receive_fn receive, void *receiver) {
  static const char reply[] = "HTTP/1.1 200 OK\r\nContent-Type: application/graphql-response+json\r\n"
                             "\r\n{\"data\":{\"viewer\":{\"name\":\"Ada\"}}}";
  (void)user; (void)uri; (void)wire; (void)n;
  return receive(receiver, reply, sizeof reply - 1);
}

int main(void) {
  mulib_muw3_client_t client = mulib_muw3_client_default();
  mulib_cdd_graphql_result_t result = {0};
  const mulib_json_t *data, *viewer;
  const char *name;
  client.http = offline;
  if (mulib_cdd_graphql_execute(&client, "http://example.test/graphql",
      "query Viewer { viewer { name } }", NULL, "Viewer", NULL, 0, &result) != 1)
    return 1;
  data = mulib_json_get(result.response.json, "data");
  viewer = mulib_json_get(data, "viewer");
  name = mulib_json_text(mulib_json_get(viewer, "name"), NULL);
  if (name) puts(name);
  mulib_cdd_graphql_result_free(&result);
  return 0;
}
