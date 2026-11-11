#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_SSE
#define MULIB_IMPL_MULTIPART
#define MULIB_IMPL_OPENAI
#define MULIB_IMPL_OPENAI_SERVER
#define MULIB_IMPL_SOCKET
#include "mulib-openai-server.h"
#include "mulib-socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int handle (void *user, const mulib_openai_server_request_t *q, mulib_openai_writer_t *w)
{
  const char response[] = "{\"id\":\"resp_example\",\"object\":\"response\",\"created_at\":0,\"status\":\"completed\","
    "\"model\":\"synthetic-model\",\"error\":null,\"incomplete_details\":null,\"output\":[{\"id\":\"msg_example\",\"type\":\"message\","
    "\"role\":\"assistant\",\"status\":\"completed\",\"content\":[{\"type\":\"output_text\",\"text\":\"Hello from MuLIB\",\"annotations\":[]}]}]}";
  mulib_json_t *json;
  int rc;
  (void)user;
  if (!strcmp (q->operation->id, "listModels")) {
    const char models[] = "{\"object\":\"list\",\"data\":[{\"id\":\"synthetic-model\",\"object\":\"model\",\"created\":0,\"owned_by\":\"example\"}]}";
    return mulib_openai_server_reply (w, 200, "application/json", NULL, 0, models, sizeof models - 1);
  }
  if (strcmp (q->operation->id, "createResponse")) return 0;
  if (mulib_json_boolean (mulib_json_get (q->json, "stream"))) {
    const char delta[] = "{\"type\":\"response.output_text.delta\",\"sequence_number\":0,\"item_id\":\"msg_example\",\"output_index\":0,\"content_index\":0,\"delta\":\"Hello from MuLIB\",\"logprobs\":[]}";
    json = mulib_json_parse (delta, sizeof delta - 1, NULL); if (!json) return -3;
    rc = mulib_openai_server_begin (w, 200, "text/event-stream", NULL, 0);
    if (rc == 1) rc = mulib_openai_server_event (w, "response.output_text.delta", NULL, json);
    mulib_json_destroy (json);
    if (rc != 1) return rc;
    {
      mulib_json_t *completed = mulib_json_new (MULIB_JSON_OBJECT);
      mulib_json_t *result = mulib_json_parse (response, sizeof response - 1, NULL);
      mulib_json_t *type = mulib_json_string ("response.completed");
      mulib_json_t *sequence = mulib_json_number (1);
      if (!completed || !result || !type || !sequence) {
        mulib_json_destroy (completed); mulib_json_destroy (result);
        mulib_json_destroy (type); mulib_json_destroy (sequence); return -3;
      }
      if (!mulib_json_set (completed, "response", result)) { mulib_json_destroy (result); rc = -3; }
      if (!mulib_json_set (completed, "type", type)) { mulib_json_destroy (type); rc = -3; }
      if (!mulib_json_set (completed, "sequence_number", sequence)) { mulib_json_destroy (sequence); rc = -3; }
      if (rc == 1) rc = mulib_openai_server_event (w, "response.completed", NULL, completed);
      mulib_json_destroy (completed);
    }
    return rc == 1 ? mulib_openai_server_done (w, 0) : rc;
  }
  return mulib_openai_server_reply (w, 200, "application/json", NULL, 0, response, sizeof response - 1);
}
static int stdout_sink (void *u, const void *p, size_t n)
{ (void)u; return fwrite (p, 1, n, stdout) == n; }
static int socket_sink (void *u, const void *bytes, size_t n)
{
  mulib_socket_t *s = (mulib_socket_t *)u;
  const unsigned char *p = (const unsigned char *)bytes;
  while (n) {
    size_t sent;
    int rc;
    if (mulib_socket_wait (s, 1, 30000)) return 0;
    rc = mulib_socket_send (s, p, n, &sent);
    if (rc == MULIB_SOCKET_AGAIN) continue;
    if (rc || !sent) return 0;
    p += sent; n -= sent;
  }
  return 1;
}
int main (int argc, char **argv)
{
  mulib_openai_server_t server = mulib_openai_server_default (handle, NULL);
  server.api_key = getenv ("OPENAI_API_KEY");
  if (argc == 1) {
    const char request[] = "GET /v1/models HTTP/1.1\r\nHost: localhost\r\n\r\n";
    server.api_key = NULL;
    return mulib_openai_server_process (&server, request, sizeof request - 1, NULL, stdout_sink, NULL) == 1 ? 0 : 1;
  }
  if (argc == 3 && !strcmp (argv[1], "--listen")) {
    mulib_socket_t listener = MULIB_SOCKET_INIT;
    unsigned port;
    char *end; unsigned long requested = strtoul (argv[2], &end, 10);
    if (!*argv[2] || *end || requested > 65535 || mulib_socket_listen (&listener, "127.0.0.1", (unsigned)requested, &port)) return 1;
    printf ("Listening on http://127.0.0.1:%u/v1\n", port); fflush (stdout);
    for (;;) {
      mulib_socket_t client = MULIB_SOCKET_INIT;
      mulib_buffer_t input = {0};
      if (mulib_socket_wait (&listener, 0, 30000)) continue;
      if (mulib_socket_accept (&listener, &client)) continue;
      for (;;) {
        unsigned char bytes[8192]; size_t n;
        int rc;
        if (mulib_socket_wait (&client, 0, 30000)) break;
        rc = mulib_socket_receive (&client, bytes, sizeof bytes, &n);
        if (rc == MULIB_SOCKET_AGAIN) continue;
        if (rc || input.length + n > 17u * 1024u * 1024u || !mulib_buffer_append (&input, bytes, n)) break;
        rc = mulib_openai_server_process (&server, input.data, input.length, NULL, socket_sink, &client);
        if (rc != MULIB_HTTP_MORE) break;
      }
      mulib_buffer_free (&input); mulib_socket_close (&client);
    }
  }
  fprintf (stderr, "usage: %s [--listen PORT]\n", argv[0]); return 2;
}
