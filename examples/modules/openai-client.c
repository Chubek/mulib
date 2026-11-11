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
#define MULIB_IMPL_SSE
#define MULIB_IMPL_MULTIPART
#define MULIB_IMPL_OPENAI
#define MULIB_IMPL_OPENAI_CLIENT
#include "detail/mulib-tls-implementation.h"
#include "mulib-openai-client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int offline (void *u, const mulib_uri_t *uri, const void *request, size_t n,
    mulib_webclient_receive_fn receive, void *receiver)
{
  const char wire[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
    "{\"id\":\"resp_example\",\"output\":[{\"type\":\"message\",\"content\":[{\"type\":\"output_text\",\"text\":\"Hello from MuLIB\"}]}]}";
  (void)u; (void)uri; (void)request; (void)n;
  return receive (receiver, wire, sizeof wire - 1);
}
int main (int argc, char **argv)
{
  mulib_openai_client_t client = mulib_openai_client_default (getenv ("OPENAI_API_KEY"));
  mulib_openai_response_t response = {0};
  mulib_json_t *body, *text;
  int rc;
  if (argc != 1 && argc != 3) { fprintf (stderr, "usage: %s [MODEL PROMPT]\n", argv[0]); return 2; }
  if (argc == 1) { client.base_url = "http://fixture/v1"; client.transport.http = offline; }
  else if (getenv ("OPENAI_BASE_URL")) client.base_url = getenv ("OPENAI_BASE_URL");
  body = mulib_openai_responses_request (argc == 3 ? argv[1] : "synthetic-model", argc == 3 ? argv[2] : "Hello");
  if (!body) return 1;
  rc = mulib_openai_client_json (&client, "createResponse", body, &response);
  mulib_json_destroy (body);
  if (rc != 1) {
    fprintf (stderr, "%s (HTTP %u)\n", mulib_openai_strerror (rc), response.http.message.status);
    mulib_openai_response_free (&response); return 1;
  }
  text = mulib_openai_output_text (response.json);
  if (text) {
    size_t n; const char *p = mulib_json_text (text, &n);
    (void)fwrite (p, 1, n, stdout); putchar ('\n'); mulib_json_destroy (text);
  }
  mulib_openai_response_free (&response); return 0;
}
