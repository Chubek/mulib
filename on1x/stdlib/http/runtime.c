#include "runtime.h"
#include "on1x_runtime.h"
#include "mulib-muw3.h"
#include <arpa/inet.h>
#include <string.h>

enum { url_limit = 2048, header_limit = 8192, body_limit = 32768,
       wire_limit = 65536, reply_limit = 5 + header_limit + 2 * body_limit };

static int endpoint(const char *url, char address[INET_ADDRSTRLEN],
                    unsigned *port, int *tls)
{
  mulib_uri_t uri;
  struct in_addr ip;
  char name[INET_ADDRSTRLEN];
  size_t n = 0;
  if (!url) return 0;
  while (n <= url_limit && url[n]) ++n;
  if (n > url_limit || !mulib_uri_parse(url, n, &uri) ||
      !uri.has_authority || uri.has_userinfo || uri.has_fragment ||
      !uri.host.length || uri.host.length >= sizeof name)
    return 0;
  if (uri.scheme.length == 4 && !memcmp(uri.scheme.data, "http", 4))
    *tls = 0;
  else if (uri.scheme.length == 5 && !memcmp(uri.scheme.data, "https", 5))
    *tls = 1;
  else
    return 0;
  memcpy(name, uri.host.data, uri.host.length);
  name[uri.host.length] = '\0';
  if (inet_pton(AF_INET, name, &ip) != 1 ||
      !inet_ntop(AF_INET, &ip, address, INET_ADDRSTRLEN) || strcmp(name, address))
    return 0;
  *port = *tls ? 443u : 80u;
  if (uri.has_port) {
    if (!uri.port.length || uri.port.length > 5 || uri.port.data[0] == '0')
      return 0;
    *port = 0;
    for (size_t i = 0; i < uri.port.length; ++i) {
      unsigned digit = (unsigned char)uri.port.data[i] - (unsigned)'0';
      if (digit > 9) return 0;
      *port = *port * 10 + digit;
    }
    if (*port > 65535u) return 0;
  }
  return 1;
}

static const char *error_reply(int error)
{
  switch (error) {
  case MULIB_WEBCLIENT_INVALID: return "-1";
  case MULIB_WEBCLIENT_LIMIT: return "-2";
  case MULIB_WEBCLIENT_NOMEM: return "-3";
  case MULIB_WEBCLIENT_TRANSPORT: return "-4";
  default: return "-6";
  }
}

static size_t pack_reply(const mulib_http_message_t *message, char *reply)
{
  static const char hex[] = "0123456789abcdef";
  size_t n = 4;
  unsigned status = message->status;
  if (status < 100 || status > 599 || message->body_length > body_limit)
    return 0;
  reply[0] = (char)('0' + status / 100);
  reply[1] = (char)('0' + (status / 10) % 10);
  reply[2] = (char)('0' + status % 10);
  reply[3] = '\n';
  for (size_t i = 0; i < message->header_count; ++i) {
    const char *name = message->headers[i].name;
    const char *value = message->headers[i].value;
    size_t a = strlen(name), b = strlen(value);
    /* The parser excludes CR, LF and NUL within values, and validates names. */
    if (a + b + 2 > header_limit - (n - 4)) return 0;
    memcpy(reply + n, name, a);
    n += a;
    reply[n++] = ':';
    memcpy(reply + n, value, b);
    n += b;
    reply[n++] = '\n';
  }
  reply[n++] = '\n';
  for (size_t i = 0; i < message->body_length; ++i) {
    unsigned byte = message->body[i];
    reply[n++] = hex[byte >> 4];
    reply[n++] = hex[byte & 15];
  }
  reply[n] = '\0';
  return n;
}

const char *on1x_http_exchange_v1(const char *url, int64_t head)
{
  char address[INET_ADDRSTRLEN], reply[reply_limit + 1];
  unsigned port;
  int tls;
  const char *method = head ? "HEAD" : "GET";
  const mulib_webclient_options_t options = {
    {header_limit, 64, body_limit, MULIB_HTTP_CHUNK_METADATA}, wire_limit
  };
  on1x_http_grant_v1 grant = {0};
  mulib_http_message_t message = {0};
  mulib_webclient_result result;
  size_t len;

  if ((head != 0 && head != 1) || !endpoint(url, address, &port, &tls))
    return "-1";
  if (on1x_http_authorize_v1(url, address, port, tls, (int)head, &grant) != 1 ||
      !grant.wait_ms || grant.wait_ms > 30000 ||
      (tls && (!grant.trust || !grant.trust->count)))
    return "-5";

  if (tls) {
    mulib_tls_options_t security = {.trust = grant.trust, .timeout_ms = grant.wait_ms};
    result = mulib_https_request(url, method, NULL, 0, NULL, 0, &options,
                                mulib_tls_connect, &security, &message);
  } else {
    result = mulib_webclient_request(url, method, NULL, 0, NULL, 0, &options,
                                     mulib_muw3_transport, &grant.wait_ms, &message);
  }
  if (result != MULIB_WEBCLIENT_OK) return error_reply(result);
  len = pack_reply(&message, reply);
  mulib_http_message_free(&message);
  /* No native resources are live when the On1x text allocator runs. */
  return len ? on1x_text_slice(reply, 0, (int64_t)len) : "-6";
}
