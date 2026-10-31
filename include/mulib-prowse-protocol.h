/** @file mulib-prowse-protocol.h
 * @brief CDP/WebSocket, W3C WebDriver/HTTP and Firefox Marionette browser clients.
 * Define MULIB_IMPL_PROWSE_PROTOCOL and explicit dependency switches. Native
 * POSIX transport supports localhost/numeric IPv4/IPv6; custom I/O supports TLS/DNS.
 * All operations are synchronous, bounded, and externally serialized. */
#ifndef MULIB_PROWSE_PROTOCOL_H
#define MULIB_PROWSE_PROTOCOL_H
#include "mulib-json.h"
#include "mulib-http.h"
#include "mulib-socket.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_prowse_protocol_kind
{ MULIB_PROWSE_CDP = 1, MULIB_PROWSE_WEBDRIVER, MULIB_PROWSE_MARIONETTE } mulib_prowse_protocol_kind_t;
typedef enum mulib_prowse_status
{
  MULIB_PROWSE_OK = 0, MULIB_PROWSE_INVALID = -1, MULIB_PROWSE_MEMORY = -2,
  MULIB_PROWSE_LIMIT = -3, MULIB_PROWSE_IO = -4, MULIB_PROWSE_TIMEOUT = -5,
  MULIB_PROWSE_CLOSED = -6, MULIB_PROWSE_FORMAT = -7, MULIB_PROWSE_REMOTE = -8,
  MULIB_PROWSE_UNSUPPORTED = -9, MULIB_PROWSE_STATE = -10, MULIB_PROWSE_BUSY = -11
} mulib_prowse_status_t;
typedef struct mulib_prowse_protocol mulib_prowse_protocol_t;
/** @brief Stream adapter. open/close operate on user-owned context; read/write
 * return zero with positive progress or a negative prowse error. read returns
 * CLOSED at EOF. Callbacks honor timeout_ms, never reenter the client, and remain
 * alive until destroy. random supplies cryptographic mask/nonce bytes for WS. */
typedef struct mulib_prowse_io
{
  void *user;
  int (*open) (void *, const char *host, unsigned port, int tls, unsigned timeout_ms);
  int (*read) (void *, void *, size_t, size_t *, unsigned timeout_ms);
  int (*write) (void *, const void *, size_t, size_t *, unsigned timeout_ms);
  void (*close) (void *);
  int (*random) (void *, void *, size_t);
} mulib_prowse_io_t;
typedef struct mulib_prowse_protocol_options
{
  size_t message_bytes, queued_events, messages_per_call;
  unsigned timeout_ms;
} mulib_prowse_protocol_options_t;
/** @brief Reply owns json; value borrows its protocol-specific result. Initialize
 * to zero and free after both success and REMOTE. Other failures preserve it. */
typedef struct mulib_prowse_reply
{
  mulib_json_t *json;
  const mulib_json_t *value;
  unsigned http_status;
  uint32_t id;
  int remote_error;
} mulib_prowse_reply_t;
/** @brief Open a ws:// target (CDP), http:// driver base, or marionette:// host.
 * NULL io selects native transport. Zero options select 8 MiB, 64 events,
 * 1024 messages per call and 10000 ms. Driver HTTP connects per request.
 * Returns NULL and writes error on failure. No browser process is launched. */
mulib_prowse_protocol_t *mulib_prowse_protocol_connect (mulib_prowse_protocol_kind_t,
    const char *endpoint, const mulib_prowse_protocol_options_t *, const mulib_prowse_io_t *, int *error);
/** @brief Close transport and free queued events. Does not delete browser sessions. */
void mulib_prowse_protocol_destroy (mulib_prowse_protocol_t *);
/** @brief Return the selected protocol, or zero for NULL. */
mulib_prowse_protocol_kind_t mulib_prowse_protocol_kind (const mulib_prowse_protocol_t *);
/** @brief Set the flattened CDP session ID (NULL clears it), copying the string. */
int mulib_prowse_protocol_session (mulib_prowse_protocol_t *, const char *session_id);
/** @brief Send CDP method/params or Marionette command; return the matching reply.
 * Events are queued, oldest evicted at capacity. Late/mismatched replies fail and
 * close the stream. Transport/format/timeout failures require a new connection. */
int mulib_prowse_protocol_call (mulib_prowse_protocol_t *, const char *method,
    const mulib_json_t *params, mulib_prowse_reply_t *reply);
/** @brief HTTP JSON request relative to the WebDriver base path. GET/POST/DELETE
 * supported. A non-2xx JSON response returns REMOTE with an owned reply.
 * Can also retrieve /status or Chrome /json/list through an HTTP client. */
int mulib_prowse_protocol_http (mulib_prowse_protocol_t *, const char *method,
    const char *path, const mulib_json_t *body, mulib_prowse_reply_t *reply);
/** @brief Pop an owned queued event (STATE if none). No transport I/O. */
int mulib_prowse_protocol_event (mulib_prowse_protocol_t *, mulib_json_t **event);
/** @brief Number of events discarded by the bounded queue. */
uint64_t mulib_prowse_protocol_dropped (const mulib_prowse_protocol_t *);
/** @brief Query/set request timeout, returning the old value; zero only queries.
 * Returns zero for NULL/busy clients. */
unsigned mulib_prowse_protocol_timeout (mulib_prowse_protocol_t *, unsigned milliseconds);
/** @brief Free/reset a reply. */
void mulib_prowse_reply_free (mulib_prowse_reply_t *);
/** @brief Stable explanation for a status code. */
const char *mulib_prowse_strerror (int status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PROWSE_PROTOCOL) && !defined(MULIB_PROWSE_PROTOCOL_IMPLEMENTED)
#define MULIB_PROWSE_PROTOCOL_IMPLEMENTED
#include "mulib-base64.h"
#include "mulib-git.h"
#include "mulib-utf8.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#if defined(__unix__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#endif
struct mulib_prowse_protocol
{
  mulib_prowse_protocol_kind_t kind;
  mulib_prowse_protocol_options_t options;
  mulib_prowse_io_t io;
  mulib_socket_t socket;
  char host[256], authority[280], path[2048], session[256];
  unsigned port;
  int tls, opened, busy, dead;
  uint32_t next_id;
  uint64_t deadline, dropped;
  mulib_buffer_t input;
  mulib_json_t **events;
  size_t event_count;
};
const char *mulib_prowse_strerror (int rc)
{
  switch (rc)
    {
    case 0: return "success";
    case MULIB_PROWSE_INVALID: return "invalid argument";
    case MULIB_PROWSE_MEMORY: return "allocation failed";
    case MULIB_PROWSE_LIMIT: return "resource limit";
    case MULIB_PROWSE_IO: return "transport failure";
    case MULIB_PROWSE_TIMEOUT: return "request timed out";
    case MULIB_PROWSE_CLOSED: return "connection closed";
    case MULIB_PROWSE_FORMAT: return "malformed protocol message";
    case MULIB_PROWSE_REMOTE: return "browser reported an error";
    case MULIB_PROWSE_UNSUPPORTED: return "unsupported transport or operation";
    case MULIB_PROWSE_STATE: return "invalid session state";
    case MULIB_PROWSE_BUSY: return "client is busy";
    default: return "unknown error";
    }
}
static uint64_t mulib_prowse_now_ (void)
{
  struct timespec t;
#if defined(CLOCK_MONOTONIC)
  if (!clock_gettime (CLOCK_MONOTONIC, &t))
    return (uint64_t)t.tv_sec * 1000 + (unsigned long)t.tv_nsec / 1000000;
#endif
  if (timespec_get (&t, TIME_UTC) != TIME_UTC) return 0;
  return (uint64_t)t.tv_sec * 1000 + (unsigned long)t.tv_nsec / 1000000;
}
static unsigned mulib_prowse_remaining_ (mulib_prowse_protocol_t *c)
{
  uint64_t now = mulib_prowse_now_ (), left = now < c->deadline ? c->deadline - now : 0;
  return left > c->options.timeout_ms ? c->options.timeout_ms : (unsigned)left;
}
static int mulib_prowse_random_ (void *user, void *out, size_t n)
{
  (void)user;
#if defined(__unix__) || defined(__APPLE__)
  {
    size_t at = 0;
    int fd = open ("/dev/urandom", O_RDONLY);
    if (fd < 0) return MULIB_PROWSE_IO;
    while (at < n)
      {
        ssize_t got = read (fd, (unsigned char *)out + at, n - at);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) { close (fd); return MULIB_PROWSE_IO; }
        at += (size_t)got;
      }
    close (fd); return 0;
  }
#else
  (void)out; (void)n; return MULIB_PROWSE_UNSUPPORTED;
#endif
}
#if defined(__unix__) || defined(__APPLE__)
static int mulib_prowse_poll_ (int fd, short events, unsigned ms)
{
  struct pollfd p;
  uint64_t end = mulib_prowse_now_ () + ms;
  int rc;
  p.fd = fd; p.events = events; p.revents = 0;
  do
    {
      uint64_t now = mulib_prowse_now_ ();
      unsigned left = end > now ? (unsigned)(end - now) : 0;
      rc = poll (&p, 1, left > INT_MAX ? INT_MAX : (int)left);
    }
  while (rc < 0 && errno == EINTR);
  if (!rc) return MULIB_PROWSE_TIMEOUT;
  return rc < 0 || (p.revents & POLLNVAL) ? MULIB_PROWSE_IO : 0;
}
#endif
static int mulib_prowse_native_open_ (void *user, const char *host, unsigned port, int tls, unsigned ms)
{
  mulib_prowse_protocol_t *c = (mulib_prowse_protocol_t *)user;
  if (tls) return MULIB_PROWSE_UNSUPPORTED;
#if defined(__unix__) || defined(__APPLE__)
  {
    struct sockaddr_storage storage;
    struct sockaddr_in *v4 = (struct sockaddr_in *)&storage;
    struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&storage;
    socklen_t length;
    int fd, rc, error = 0;
    socklen_t error_length = sizeof (error);
    memset (&storage, 0, sizeof (storage));
    if (!strcmp (host, "localhost")) host = "127.0.0.1";
    if (inet_pton (AF_INET, host, &v4->sin_addr) == 1)
      { v4->sin_family = AF_INET; v4->sin_port = htons ((uint16_t)port); length = sizeof (*v4); }
    else if (inet_pton (AF_INET6, host, &v6->sin6_addr) == 1)
      { v6->sin6_family = AF_INET6; v6->sin6_port = htons ((uint16_t)port); length = sizeof (*v6); }
    else return MULIB_PROWSE_UNSUPPORTED;
    fd = socket (storage.ss_family, SOCK_STREAM, 0); if (fd < 0) return MULIB_PROWSE_IO;
    c->socket.fd = fd;
    if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0 || mulib_socket_nonblocking (&c->socket, 1))
      { mulib_socket_close (&c->socket); return MULIB_PROWSE_IO; }
    rc = connect (fd, (struct sockaddr *)&storage, length);
    if (rc < 0 && errno != EINPROGRESS) { mulib_socket_close (&c->socket); return MULIB_PROWSE_IO; }
    rc = mulib_prowse_poll_ (fd, POLLOUT, ms);
    if (!rc && (getsockopt (fd, SOL_SOCKET, SO_ERROR, &error, &error_length) < 0 || error)) rc = MULIB_PROWSE_IO;
    if (rc) mulib_socket_close (&c->socket);
    return rc;
  }
#else
  (void)c; (void)host; (void)port; (void)ms; return MULIB_PROWSE_UNSUPPORTED;
#endif
}
static int mulib_prowse_native_read_ (void *user, void *out, size_t n, size_t *got, unsigned ms)
{
  mulib_prowse_protocol_t *c = (mulib_prowse_protocol_t *)user;
#if defined(__unix__) || defined(__APPLE__)
  int rc = mulib_prowse_poll_ (c->socket.fd, POLLIN, ms);
  if (rc) return rc;
  rc = mulib_socket_receive (&c->socket, out, n, got);
  return rc == MULIB_SOCKET_EOF ? MULIB_PROWSE_CLOSED : rc ? MULIB_PROWSE_IO : 0;
#else
  (void)c; (void)out; (void)n; (void)got; (void)ms; return MULIB_PROWSE_UNSUPPORTED;
#endif
}
static int mulib_prowse_native_write_ (void *user, const void *data, size_t n, size_t *sent, unsigned ms)
{
  mulib_prowse_protocol_t *c = (mulib_prowse_protocol_t *)user;
#if defined(__unix__) || defined(__APPLE__)
  int rc = mulib_prowse_poll_ (c->socket.fd, POLLOUT, ms);
  if (rc) return rc;
  return mulib_socket_send (&c->socket, data, n, sent) ? MULIB_PROWSE_IO : 0;
#else
  (void)c; (void)data; (void)n; (void)sent; (void)ms; return MULIB_PROWSE_UNSUPPORTED;
#endif
}
static void mulib_prowse_native_close_ (void *user)
{ (void)mulib_socket_close (&((mulib_prowse_protocol_t *)user)->socket); }
static void mulib_prowse_close_ (mulib_prowse_protocol_t *c)
{ if (c->opened) c->io.close (c->io.user); c->opened = 0; c->input.length = 0; }
static int mulib_prowse_open_ (mulib_prowse_protocol_t *c)
{
  unsigned ms = mulib_prowse_remaining_ (c);
  int rc;
  if (!ms) return MULIB_PROWSE_TIMEOUT;
  rc = c->io.open (c->io.user, c->host, c->port, c->tls, ms);
  if (!rc) c->opened = 1;
  return rc > 0 ? MULIB_PROWSE_IO : rc;
}
static int mulib_prowse_write_ (mulib_prowse_protocol_t *c, const void *data, size_t n)
{
  size_t at = 0;
  while (at < n)
    {
      size_t sent = 0;
      unsigned ms = mulib_prowse_remaining_ (c);
      int rc;
      if (!ms) return MULIB_PROWSE_TIMEOUT;
      rc = c->io.write (c->io.user, (const unsigned char *)data + at, n - at, &sent, ms);
      if (rc) return rc < 0 ? rc : MULIB_PROWSE_IO;
      if (!sent || sent > n - at) return MULIB_PROWSE_IO;
      at += sent;
    }
  return 0;
}
static int mulib_prowse_read_more_ (mulib_prowse_protocol_t *c)
{
  unsigned char data[4096];
  size_t got = 0, cap = c->options.message_bytes + 32768 - c->input.length;
  unsigned ms = mulib_prowse_remaining_ (c);
  int rc;
  if (!ms) return MULIB_PROWSE_TIMEOUT;
  if (!cap) return MULIB_PROWSE_LIMIT;
  if (cap > sizeof (data)) cap = sizeof (data);
  rc = c->io.read (c->io.user, data, cap, &got, ms);
  if (rc) return rc < 0 ? rc : MULIB_PROWSE_IO;
  if (!got || got > cap) return MULIB_PROWSE_IO;
  return mulib_buffer_append (&c->input, data, got) ? 0 : MULIB_PROWSE_MEMORY;
}
static int mulib_prowse_need_ (mulib_prowse_protocol_t *c, size_t n)
{ int rc; while (c->input.length < n) { rc = mulib_prowse_read_more_ (c); if (rc) return rc; } return 0; }
static void mulib_prowse_consume_ (mulib_prowse_protocol_t *c, size_t n)
{ c->input.length -= n; memmove (c->input.data, c->input.data + n, c->input.length); }
static int mulib_prowse_http_read_ (mulib_prowse_protocol_t *c, mulib_http_message_t *out)
{
  mulib_http_options_t options = { 32768, 128, 0, 0 };
  size_t used;
  int rc;
  options.max_body_bytes = c->options.message_bytes;
  for (;;)
    {
      rc = mulib_http_parse (c->input.data, c->input.length, 1, &options, out, &used);
      if (rc == MULIB_HTTP_OK) { mulib_prowse_consume_ (c, used); return 0; }
      if (rc < 0) return rc == MULIB_HTTP_NOMEM ? MULIB_PROWSE_MEMORY : rc == MULIB_HTTP_LIMIT ? MULIB_PROWSE_LIMIT : MULIB_PROWSE_FORMAT;
      if (options.flags & MULIB_HTTP_EOF) return MULIB_PROWSE_FORMAT;
      rc = mulib_prowse_read_more_ (c);
      if (rc == MULIB_PROWSE_CLOSED) options.flags |= MULIB_HTTP_EOF;
      else if (rc) return rc;
    }
}
static int mulib_prowse_ws_send_ (mulib_prowse_protocol_t *c, unsigned opcode, const void *data, size_t n)
{
  unsigned char header[14], mask[4], chunk[4096];
  size_t h = 2, at = 0, i;
  int rc;
  if (n > c->options.message_bytes) return MULIB_PROWSE_LIMIT;
  rc = c->io.random (c->io.user, mask, 4); if (rc) return rc < 0 ? rc : MULIB_PROWSE_IO;
  header[0] = (unsigned char)(0x80 | opcode);
  if (n < 126) header[1] = (unsigned char)(0x80 | n);
  else if (n <= 65535) { header[1] = 0xfe; header[h++] = (unsigned char)(n >> 8); header[h++] = (unsigned char)n; }
  else { header[1] = 0xff; for (i = 0; i < 8; ++i) header[h++] = (unsigned char)((uint64_t)n >> (56 - i * 8)); }
  memcpy (header + h, mask, 4); h += 4; rc = mulib_prowse_write_ (c, header, h);
  while (!rc && at < n)
    {
      size_t take = n - at < sizeof (chunk) ? n - at : sizeof (chunk);
      for (i = 0; i < take; ++i) chunk[i] = ((const unsigned char *)data)[at + i] ^ mask[(at + i) % 4];
      rc = mulib_prowse_write_ (c, chunk, take); at += take;
    }
  return rc;
}
static int mulib_prowse_ws_receive_ (mulib_prowse_protocol_t *c, mulib_buffer_t *text)
{
  size_t frames;
  int fragmented = 0;
  for (frames = 0; frames < c->options.messages_per_call; ++frames)
    {
      size_t h = 2, i;
      uint64_t n;
      unsigned opcode, final, marker;
      int rc = mulib_prowse_need_ (c, 2);
      if (rc) return rc;
      opcode = c->input.data[0] & 15; final = c->input.data[0] & 128; marker = c->input.data[1] & 127;
      if ((c->input.data[0] & 112) || (c->input.data[1] & 128)) return MULIB_PROWSE_FORMAT;
      n = marker;
      if (marker >= 126)
        {
          h += marker == 126 ? 2 : 8; rc = mulib_prowse_need_ (c, h); if (rc) return rc;
          n = 0; for (i = 2; i < h; ++i) n = (n << 8) | c->input.data[i];
          if ((marker == 126 && n < 126) || (marker == 127 && (n < 65536 || n >> 63))) return MULIB_PROWSE_FORMAT;
        }
      if (n > c->options.message_bytes || n > SIZE_MAX - h) return MULIB_PROWSE_LIMIT;
      if (opcode >= 8 && (!final || n > 125)) return MULIB_PROWSE_FORMAT;
      rc = mulib_prowse_need_ (c, h + (size_t)n); if (rc) return rc;
      if (opcode == 8)
        {
          if (n == 1) return MULIB_PROWSE_FORMAT;
          if (n >= 2)
            {
              unsigned status = ((unsigned)c->input.data[h] << 8) | c->input.data[h + 1];
              if (status < 1000 || status >= 5000 || status == 1004 || status == 1005
                  || status == 1006 || status == 1015 || (status >= 1016 && status < 3000)
                  || !mulib_utf8_validate (c->input.data + h + 2, (size_t)n - 2)) return MULIB_PROWSE_FORMAT;
            }
          (void)mulib_prowse_ws_send_ (c, 8, c->input.data + h, (size_t)n);
          return MULIB_PROWSE_CLOSED;
        }
      if (opcode == 9) { rc = mulib_prowse_ws_send_ (c, 10, c->input.data + h, (size_t)n); if (rc) return rc; }
      else if (opcode != 10)
        {
          if ((opcode != 0 && opcode != 1) || (opcode == 0 && !fragmented) || (opcode == 1 && fragmented)) return MULIB_PROWSE_FORMAT;
          if (n > c->options.message_bytes - text->length) return MULIB_PROWSE_LIMIT;
          if (!mulib_buffer_append (text, c->input.data + h, (size_t)n)) return MULIB_PROWSE_MEMORY;
          fragmented = !final;
        }
      mulib_prowse_consume_ (c, h + (size_t)n);
      if (opcode <= 1 && final)
        return mulib_utf8_validate (text->data, text->length) ? 0 : MULIB_PROWSE_FORMAT;
    }
  return MULIB_PROWSE_LIMIT;
}
static int mulib_prowse_marionette_receive_ (mulib_prowse_protocol_t *c, mulib_buffer_t *text)
{
  size_t at = 0, length = 0;
  int rc;
  for (;;)
    {
      unsigned char ch;
      if (at > 20) return MULIB_PROWSE_FORMAT;
      rc = mulib_prowse_need_ (c, at + 1); if (rc) return rc;
      ch = c->input.data[at++];
      if (ch == ':') break;
      if (ch < '0' || ch > '9') return MULIB_PROWSE_FORMAT;
      if (length > (c->options.message_bytes - (ch - '0')) / 10) return MULIB_PROWSE_LIMIT;
      length = length * 10 + ch - '0';
    }
  if (at == 1 || !length) return MULIB_PROWSE_FORMAT;
  rc = mulib_prowse_need_ (c, at + length); if (rc) return rc;
  if (!mulib_buffer_append (text, c->input.data + at, length)) return MULIB_PROWSE_MEMORY;
  mulib_prowse_consume_ (c, at + length); return 0;
}
static int mulib_prowse_token_ (const char *value, const char *wanted)
{
  size_t n = strlen (wanted);
  if (!value) return 0;
  while (*value)
    {
      size_t i, length;
      while (*value == ' ' || *value == '\t' || *value == ',') ++value;
      length = strcspn (value, ", \t");
      for (i = 0; i < length && i < n; ++i)
        {
          unsigned char ch = (unsigned char)value[i];
          if (ch >= 'A' && ch <= 'Z') ch = (unsigned char)(ch + 'a' - 'A');
          if (ch != (unsigned char)wanted[i]) break;
        }
      if (i == n && length == n) return 1;
      value += length;
    }
  return 0;
}
static int mulib_prowse_ws_open_ (mulib_prowse_protocol_t *c)
{
  unsigned char nonce[16];
  char key[25], expected[29], joined[61], request[2600];
  mulib_git_oid_t hash;
  mulib_http_message_t response = { 0 };
  const char *accept, *upgrade, *connection;
  int rc, count;
  rc = c->io.random (c->io.user, nonce, sizeof (nonce)); if (rc) return rc < 0 ? rc : MULIB_PROWSE_IO;
  if (!mulib_base64_encode (nonce, sizeof (nonce), key, sizeof (key))) return MULIB_PROWSE_FORMAT;
  (void)snprintf (joined, sizeof (joined), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
  if (!mulib_git_sha1 (joined, strlen (joined), &hash)
      || !mulib_base64_encode (hash.bytes, 20, expected, sizeof (expected))) return MULIB_PROWSE_FORMAT;
  count = snprintf (request, sizeof (request), "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: %s\r\n\r\n", c->path, c->authority, key);
  if (count < 0 || (size_t)count >= sizeof (request)) return MULIB_PROWSE_LIMIT;
  rc = mulib_prowse_write_ (c, request, (size_t)count);
  if (!rc) rc = mulib_prowse_http_read_ (c, &response);
  accept = mulib_http_header (&response, "Sec-WebSocket-Accept");
  upgrade = mulib_http_header (&response, "Upgrade"); connection = mulib_http_header (&response, "Connection");
  if (!rc && (response.status != 101 || !accept || strcmp (accept, expected)
      || !mulib_prowse_token_ (upgrade, "websocket") || !mulib_prowse_token_ (connection, "upgrade")
      || mulib_http_header (&response, "Sec-WebSocket-Extensions")
      || mulib_http_header (&response, "Sec-WebSocket-Protocol"))) rc = MULIB_PROWSE_FORMAT;
  mulib_http_message_free (&response); return rc;
}
static int mulib_prowse_json_ (const char *text, size_t n, mulib_json_t **out)
{
  mulib_json_error_t error = { 0, 0, 0, NULL };
  *out = mulib_json_parse (text, n, &error);
  if (*out) return 0;
  return error.message && (!strcmp (error.message, "out of memory")
      || !strcmp (error.message, "capture allocation overflow")) ? MULIB_PROWSE_MEMORY : MULIB_PROWSE_FORMAT;
}
mulib_prowse_protocol_t *mulib_prowse_protocol_connect (mulib_prowse_protocol_kind_t kind,
    const char *endpoint, const mulib_prowse_protocol_options_t *options, const mulib_prowse_io_t *io, int *error)
{
  mulib_prowse_protocol_t *c = NULL;
  mulib_uri_t uri;
  size_t i, start = 0, hostlen;
  int rc = MULIB_PROWSE_INVALID;
  if (error) *error = rc;
  if (!endpoint || kind < 1 || kind > MULIB_PROWSE_MARIONETTE
      || !mulib_uri_parse (endpoint, strlen (endpoint), &uri) || !uri.has_authority
      || uri.has_userinfo || uri.has_fragment || !uri.host.length) return NULL;
  if (kind == MULIB_PROWSE_CDP)
    { if (!((uri.scheme.length == 2 && !memcmp (uri.scheme.data, "ws", 2)) || (uri.scheme.length == 3 && !memcmp (uri.scheme.data, "wss", 3)))) return NULL; }
  else if (kind == MULIB_PROWSE_WEBDRIVER)
    { if (!((uri.scheme.length == 4 && !memcmp (uri.scheme.data, "http", 4)) || (uri.scheme.length == 5 && !memcmp (uri.scheme.data, "https", 5))) || uri.has_query) return NULL; }
  else if (uri.scheme.length != 10 || memcmp (uri.scheme.data, "marionette", 10) || uri.has_query || uri.path.length) return NULL;
  c = (mulib_prowse_protocol_t *)calloc (1, sizeof (*c)); if (!c) { if (error) *error = MULIB_PROWSE_MEMORY; return NULL; }
  c->socket.fd = -1; c->kind = kind; c->next_id = 1;
  if (options) c->options = *options;
  if (!c->options.message_bytes) c->options.message_bytes = 8 * 1024 * 1024;
  if (!c->options.queued_events) c->options.queued_events = 64;
  if (!c->options.messages_per_call) c->options.messages_per_call = 1024;
  if (!c->options.timeout_ms) c->options.timeout_ms = 10000;
  if (c->options.message_bytes < 16 || c->options.message_bytes > SIZE_MAX - 32768
      || c->options.queued_events > SIZE_MAX / sizeof (*c->events)) { rc = MULIB_PROWSE_LIMIT; goto fail; }
  c->tls = (kind == MULIB_PROWSE_CDP && uri.scheme.length == 3) || (kind == MULIB_PROWSE_WEBDRIVER && uri.scheme.length == 5);
  c->port = c->tls ? 443 : kind == MULIB_PROWSE_MARIONETTE ? 2828 : 80;
  if (uri.has_port)
    {
      c->port = 0;
      for (i = 0; i < uri.port.length; ++i)
        { unsigned ch = (unsigned char)uri.port.data[i]; if (ch < '0' || ch > '9' || c->port > (65535 - (ch - '0')) / 10) goto fail; c->port = c->port * 10 + ch - '0'; }
      if (!c->port) goto fail;
    }
  hostlen = uri.host.length;
  if (uri.host.data[0] == '[') { start = 1; hostlen -= 2; }
  if (hostlen >= sizeof (c->host) || uri.host.length > 255) { rc = MULIB_PROWSE_LIMIT; goto fail; }
  memcpy (c->host, uri.host.data + start, hostlen); c->host[hostlen] = 0;
  (void)snprintf (c->authority, sizeof (c->authority), "%.*s:%u", (int)uri.host.length, uri.host.data, c->port);
  if (uri.path.length + uri.query.length + 2 > sizeof (c->path)) { rc = MULIB_PROWSE_LIMIT; goto fail; }
  if (uri.path.length) memcpy (c->path, uri.path.data, uri.path.length);
  else strcpy (c->path, "/");
  if (uri.has_query) { strcat (c->path, "?"); strncat (c->path, uri.query.data, uri.query.length); }
  if (io) c->io = *io;
  else
    { c->io.user = c; c->io.open = mulib_prowse_native_open_; c->io.read = mulib_prowse_native_read_; c->io.write = mulib_prowse_native_write_; c->io.close = mulib_prowse_native_close_; }
  if (!c->io.random) c->io.random = mulib_prowse_random_;
  if (!c->io.open || !c->io.read || !c->io.write || !c->io.close) goto fail;
  c->events = (mulib_json_t **)calloc (c->options.queued_events, sizeof (*c->events));
  if (!c->events) { rc = MULIB_PROWSE_MEMORY; goto fail; }
  c->deadline = mulib_prowse_now_ () + c->options.timeout_ms;
  rc = kind == MULIB_PROWSE_WEBDRIVER ? 0 : mulib_prowse_open_ (c);
  if (!rc && kind == MULIB_PROWSE_CDP) rc = mulib_prowse_ws_open_ (c);
  if (!rc && kind == MULIB_PROWSE_MARIONETTE)
    {
      mulib_buffer_t hello = { 0 };
      mulib_json_t *json = NULL;
      rc = mulib_prowse_marionette_receive_ (c, &hello);
      if (!rc) rc = mulib_prowse_json_ ((char *)hello.data, hello.length, &json);
      if (!rc && (mulib_json_numeric (mulib_json_get (json, "marionetteProtocol")) != 3
          || mulib_json_typeof (json) != MULIB_JSON_OBJECT)) rc = MULIB_PROWSE_FORMAT;
      mulib_json_destroy (json); mulib_buffer_free (&hello);
    }
  if (rc) goto fail;
  if (error) *error = 0;
  return c;
fail:
  if (error) *error = rc;
  mulib_prowse_protocol_destroy (c); return NULL;
}
void mulib_prowse_protocol_destroy (mulib_prowse_protocol_t *c)
{
  size_t i;
  if (!c || c->busy) return;
  mulib_prowse_close_ (c); mulib_buffer_free (&c->input);
  for (i = 0; i < c->event_count; ++i) mulib_json_destroy (c->events[i]);
  free (c->events); free (c);
}
mulib_prowse_protocol_kind_t mulib_prowse_protocol_kind (const mulib_prowse_protocol_t *c)
{ return c ? c->kind : (mulib_prowse_protocol_kind_t)0; }
int mulib_prowse_protocol_session (mulib_prowse_protocol_t *c, const char *id)
{
  size_t n = id ? strlen (id) : 0;
  if (!c || c->kind != MULIB_PROWSE_CDP || n >= sizeof (c->session)) return MULIB_PROWSE_INVALID;
  if ((id && !n) || !mulib_utf8_validate ((const unsigned char *)id, n)) return MULIB_PROWSE_INVALID;
  if (c->busy) return MULIB_PROWSE_BUSY;
  if (n) memmove (c->session, id, n);
  c->session[n] = 0; return 0;
}
void mulib_prowse_reply_free (mulib_prowse_reply_t *r)
{ if (r) { mulib_json_destroy (r->json); memset (r, 0, sizeof (*r)); } }
static int mulib_prowse_set_ (mulib_json_t *o, const char *key, mulib_json_t *v)
{ if (v && mulib_json_set (o, key, v)) return 1; mulib_json_destroy (v); return 0; }
static int mulib_prowse_tree_ (const mulib_json_t *v, size_t depth, size_t *budget)
{
  size_t i, n;
  if (!v || depth > 64 || !*budget) return 0;
  --*budget;
  if (mulib_json_typeof (v) == MULIB_JSON_STRING)
    { (void)mulib_json_text (v, &n); if (n > *budget) return 0; *budget -= n; }
  for (i = 0; i < mulib_json_size (v); ++i)
    {
      if (mulib_json_typeof (v) == MULIB_JSON_OBJECT)
        { (void)mulib_json_key (v, i, &n); if (n > *budget) return 0; *budget -= n; }
      if (!mulib_prowse_tree_ (mulib_json_at (v, i), depth + 1, budget)) return 0;
    }
  return 1;
}
static mulib_json_t *mulib_prowse_clone_ (const mulib_json_t *v)
{
  char *text = mulib_json_stringify (v, 0);
  mulib_json_t *copy = text ? mulib_json_parse (text, strlen (text), NULL) : NULL;
  free (text); return copy;
}
static int mulib_prowse_queue_ (mulib_prowse_protocol_t *c, mulib_json_t *json)
{
  if (c->event_count == c->options.queued_events)
    {
      mulib_json_destroy (c->events[0]); --c->event_count;
      memmove (c->events, c->events + 1, c->event_count * sizeof (*c->events));
      if (c->dropped != UINT64_MAX) ++c->dropped;
    }
  c->events[c->event_count++] = json; return 0;
}
int mulib_prowse_protocol_event (mulib_prowse_protocol_t *c, mulib_json_t **out)
{
  if (!c || !out || *out) return MULIB_PROWSE_INVALID;
  if (c->busy) return MULIB_PROWSE_BUSY;
  if (!c->event_count) return MULIB_PROWSE_STATE;
  *out = c->events[0]; --c->event_count;
  memmove (c->events, c->events + 1, c->event_count * sizeof (*c->events)); return 0;
}
uint64_t mulib_prowse_protocol_dropped (const mulib_prowse_protocol_t *c)
{ return c ? c->dropped : 0; }
unsigned mulib_prowse_protocol_timeout (mulib_prowse_protocol_t *c, unsigned ms)
{
  unsigned old;
  if (!c || c->busy) return 0;
  old = c->options.timeout_ms; if (ms) c->options.timeout_ms = ms; return old;
}
int mulib_prowse_protocol_call (mulib_prowse_protocol_t *c, const char *method,
    const mulib_json_t *params, mulib_prowse_reply_t *out)
{
  mulib_json_t *request = NULL, *p = NULL;
  char *text = NULL;
  size_t i, budget;
  uint32_t id;
  int rc = 0;
  if (!c || !method || !*method || !out || out->json || (params && mulib_json_typeof (params) != MULIB_JSON_OBJECT)) return MULIB_PROWSE_INVALID;
  if (!mulib_utf8_validate ((const unsigned char *)method, strlen (method))) return MULIB_PROWSE_INVALID;
  if (c->busy) return MULIB_PROWSE_BUSY;
  if (c->kind == MULIB_PROWSE_WEBDRIVER) return MULIB_PROWSE_UNSUPPORTED;
  if (c->dead || !c->opened) return MULIB_PROWSE_CLOSED;
  budget = c->options.message_bytes;
  if (strlen (method) > budget || (params && !mulib_prowse_tree_ (params, 0, &budget)) || c->next_id > INT32_MAX) return MULIB_PROWSE_LIMIT;
  request = mulib_json_new (c->kind == MULIB_PROWSE_CDP ? MULIB_JSON_OBJECT : MULIB_JSON_ARRAY);
  p = params ? mulib_prowse_clone_ (params) : mulib_json_new (MULIB_JSON_OBJECT);
  if (!request || !p) { rc = MULIB_PROWSE_MEMORY; goto done; }
  id = c->next_id++;
  if (c->kind == MULIB_PROWSE_CDP)
    {
      if (!mulib_prowse_set_ (request, "id", mulib_json_number (id))
          || !mulib_prowse_set_ (request, "method", mulib_json_string (method))
          || (*c->session && !mulib_prowse_set_ (request, "sessionId", mulib_json_string (c->session)))) { rc = MULIB_PROWSE_MEMORY; goto done; }
      if (!mulib_json_set (request, "params", p)) { rc = MULIB_PROWSE_MEMORY; goto done; } p = NULL;
    }
  else
    {
      mulib_json_t *v[3];
      v[0] = mulib_json_number (0); v[1] = mulib_json_number (id); v[2] = mulib_json_string (method);
      for (i = 0; i < 3; ++i)
        { if (!v[i] || !mulib_json_append (request, v[i])) { size_t j; for (j = i; j < 3; ++j) mulib_json_destroy (v[j]); rc = MULIB_PROWSE_MEMORY; goto done; } }
      if (!mulib_json_append (request, p)) { rc = MULIB_PROWSE_MEMORY; goto done; } p = NULL;
    }
  text = mulib_json_stringify (request, 0); if (!text) { rc = MULIB_PROWSE_MEMORY; goto done; }
  if (strlen (text) > c->options.message_bytes) { rc = MULIB_PROWSE_LIMIT; goto done; }
  c->busy = 1; c->deadline = mulib_prowse_now_ () + c->options.timeout_ms;
  if (c->kind == MULIB_PROWSE_CDP) rc = mulib_prowse_ws_send_ (c, 1, text, strlen (text));
  else
    {
      char prefix[32]; int count = snprintf (prefix, sizeof (prefix), "%zu:", strlen (text));
      rc = mulib_prowse_write_ (c, prefix, (size_t)count);
      if (!rc) rc = mulib_prowse_write_ (c, text, strlen (text));
    }
  for (i = 0; !rc && i < c->options.messages_per_call; ++i)
    {
      mulib_buffer_t bytes = { 0 };
      mulib_json_t *json = NULL, *ident, *error, *value;
      rc = c->kind == MULIB_PROWSE_CDP ? mulib_prowse_ws_receive_ (c, &bytes) : mulib_prowse_marionette_receive_ (c, &bytes);
      if (!rc) rc = mulib_prowse_json_ ((char *)bytes.data, bytes.length, &json);
      mulib_buffer_free (&bytes);
      if (rc) break;
      if (!json) { rc = MULIB_PROWSE_FORMAT; break; }
      if (c->kind == MULIB_PROWSE_CDP)
        {
          size_t session_length = 0, method_length = 0;
          const mulib_json_t *session_value = mulib_json_get (json, "sessionId");
          const char *session = mulib_json_text (session_value, &session_length);
          const char *event_method = mulib_json_text (mulib_json_get (json, "method"), &method_length);
          ident = mulib_json_get (json, "id"); error = mulib_json_get (json, "error"); value = mulib_json_get (json, "result");
          if (session_value && (!session || !session_length || memchr (session, 0, session_length))) rc = MULIB_PROWSE_FORMAT;
          if (!ident && !rc)
            {
              const mulib_json_t *event_params = mulib_json_get (json, "params");
              if (!event_method || !method_length || memchr (event_method, 0, method_length)
                  || error || value || (event_params && mulib_json_typeof (event_params) != MULIB_JSON_OBJECT)) rc = MULIB_PROWSE_FORMAT;
              else { mulib_prowse_queue_ (c, json); continue; }
            }
          if ((*c->session && (!session || strcmp (session, c->session))) || (!*c->session && session)) rc = MULIB_PROWSE_FORMAT;
          if (mulib_json_typeof (json) != MULIB_JSON_OBJECT || (!!error == !!value)
              || mulib_json_get (json, "method") || mulib_json_get (json, "params")
              || (value && mulib_json_typeof (value) != MULIB_JSON_OBJECT)
              || (error && (mulib_json_typeof (error) != MULIB_JSON_OBJECT
                  || mulib_json_typeof (mulib_json_get (error, "code")) != MULIB_JSON_NUMBER
                  || floor (mulib_json_numeric (mulib_json_get (error, "code"))) != mulib_json_numeric (mulib_json_get (error, "code"))
                  || mulib_json_typeof (mulib_json_get (error, "message")) != MULIB_JSON_STRING))) rc = MULIB_PROWSE_FORMAT;
        }
      else
        {
          ident = mulib_json_at (json, 1); error = mulib_json_at (json, 2); value = mulib_json_at (json, 3);
          if (mulib_json_typeof (json) != MULIB_JSON_ARRAY || mulib_json_size (json) != 4
              || mulib_json_typeof (mulib_json_at (json, 0)) != MULIB_JSON_NUMBER
              || mulib_json_numeric (mulib_json_at (json, 0)) != 1
              || (mulib_json_typeof (error) != MULIB_JSON_NULL && mulib_json_typeof (error) != MULIB_JSON_OBJECT)) rc = MULIB_PROWSE_FORMAT;
          if (mulib_json_typeof (error) == MULIB_JSON_NULL) error = NULL;
        }
      if (mulib_json_typeof (ident) != MULIB_JSON_NUMBER || mulib_json_numeric (ident) != id) rc = MULIB_PROWSE_FORMAT;
      if (rc) { mulib_json_destroy (json); break; }
      out->json = json; out->value = error ? error : value; out->id = id; out->remote_error = error != NULL;
      rc = error ? MULIB_PROWSE_REMOTE : 0; goto done;
    }
  if (!rc) rc = MULIB_PROWSE_LIMIT;
done:
  if (c->busy && rc && rc != MULIB_PROWSE_REMOTE) { mulib_prowse_close_ (c); c->dead = 1; }
  c->busy = 0; free (text); mulib_json_destroy (request); mulib_json_destroy (p); return rc;
}
int mulib_prowse_protocol_http (mulib_prowse_protocol_t *c, const char *method,
    const char *path, const mulib_json_t *body, mulib_prowse_reply_t *out)
{
  mulib_http_message_t request = { 0 }, response = { 0 };
  mulib_http_header_t headers[3];
  mulib_http_options_t options = { 32768, 128, 0, 0 };
  mulib_json_t *json = NULL;
  unsigned char *wire = NULL;
  char *text = NULL, target[4096];
  size_t length, base, budget;
  int rc;
  if (!c || !method || !path || *path != '/' || !out || out->json
      || (strcmp (method, "GET") && strcmp (method, "POST") && strcmp (method, "DELETE"))) return MULIB_PROWSE_INVALID;
  if (c->busy) return MULIB_PROWSE_BUSY;
  if (c->kind != MULIB_PROWSE_WEBDRIVER) return MULIB_PROWSE_UNSUPPORTED;
  base = strlen (c->path); while (base && c->path[base - 1] == '/') --base;
  if (strlen (path) >= sizeof (target) - base) return MULIB_PROWSE_LIMIT;
  memcpy (target, c->path, base); strcpy (target + base, path);
  budget = c->options.message_bytes;
  if (body && !mulib_prowse_tree_ (body, 0, &budget)) return MULIB_PROWSE_LIMIT;
  if (body) { text = mulib_json_stringify (body, 0); if (!text) return MULIB_PROWSE_MEMORY; }
  headers[0].name = (char *)"Host"; headers[0].value = c->authority;
  headers[1].name = (char *)"Connection"; headers[1].value = (char *)"close";
  headers[2].name = (char *)"Content-Type"; headers[2].value = (char *)"application/json; charset=utf-8";
  request.method = (char *)method; request.target = target; request.headers = headers; request.header_count = 3;
  request.body = (unsigned char *)text; request.body_length = text ? strlen (text) : 0;
  options.max_body_bytes = c->options.message_bytes;
  rc = mulib_http_serialize (&request, &options, &wire, &length); free (text);
  if (rc != MULIB_HTTP_OK) return rc == MULIB_HTTP_NOMEM ? MULIB_PROWSE_MEMORY : rc == MULIB_HTTP_LIMIT ? MULIB_PROWSE_LIMIT : MULIB_PROWSE_INVALID;
  c->busy = 1; c->deadline = mulib_prowse_now_ () + c->options.timeout_ms;
  rc = mulib_prowse_open_ (c);
  if (!rc) rc = mulib_prowse_write_ (c, wire, length);
  if (!rc) rc = mulib_prowse_http_read_ (c, &response);
  free (wire); mulib_prowse_close_ (c); c->busy = 0;
  if (!rc)
    {
      rc = mulib_prowse_json_ ((char *)response.body, response.body_length, &json);
      if (!rc)
        {
          const mulib_json_t *value = mulib_json_get (json, "value");
          out->json = json; out->value = value ? value : json; out->http_status = response.status;
          out->remote_error = response.status < 200 || response.status >= 300;
          if (out->remote_error) rc = MULIB_PROWSE_REMOTE;
        }
    }
  mulib_http_message_free (&response); return rc;
}
#endif
