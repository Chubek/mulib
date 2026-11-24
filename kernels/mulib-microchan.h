/** @file mulib-microchan.h
 * @brief Versioned local publish/subscribe protocol, client and bounded broker.
 * Define MULIB_IMPL_MICROCHAN, CONTAINER_IPC, CRC32 and SOCKET once. */
#ifndef MULIB_MICROCHAN_H
#define MULIB_MICROCHAN_H
#include "mulib-container-ipc.h"
#include "mulib-socket.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_MICROCHAN_SUBSCRIBE = 0x4d01, MULIB_MICROCHAN_UNSUBSCRIBE = 0x4d02,
  MULIB_MICROCHAN_PUBLISH = 0x4d03, MULIB_MICROCHAN_EVENT = 0x4d04,
  MULIB_MICROCHAN_TOPIC_CAP = 128
};
/** Value topic + borrowed binary payload. Requests have response=0/status=0;
 * acknowledgements echo opcode/id/topic with response=1 and CT status.
 * Events have response=1/status=0 and the publisher's correlation ID. */
typedef struct mulib_microchan_message {
  unsigned opcode;
  int response, status;
  uint64_t id;
  char topic[MULIB_MICROCHAN_TOPIC_CAP];
  const unsigned char *data;
  size_t length;
} mulib_microchan_message_t;
typedef struct mulib_microchan_options {
  size_t max_payload, max_queue_bytes, max_clients, max_subscriptions;
  const char *topic_prefix; /* copied by broker; NULL/empty permits all topics */
} mulib_microchan_options_t;
typedef struct mulib_microchan_client mulib_microchan_client_t;
typedef struct mulib_microchan_broker mulib_microchan_broker_t;
/** All APIs return MULIB_CT_* (0 success, AGAIN timeout/incomplete).
 * Payload limit zero selects 1 MiB. Encode atomically replaces initialized blob;
 * decode borrows bytes and leaves output unchanged on failure, consumed=0. */
int mulib_microchan_encode (const mulib_microchan_message_t *, size_t max_payload, mulib_ct_blob *);
int mulib_microchan_decode (const void *, size_t, size_t max_payload,
                           mulib_microchan_message_t *, size_t *consumed);
/** Connect to a filesystem Unix endpoint. Output must point to NULL. */
int mulib_microchan_open (mulib_microchan_client_t **, const char *path,
                         size_t max_payload, unsigned timeout_ms);
void mulib_microchan_close (mulib_microchan_client_t *);
/** Sends a complete frame; partial send errors poison/close the connection.
 * timeout_ms is per wait, zero selects 30 seconds. IDs are caller-selected. */
int mulib_microchan_send (mulib_microchan_client_t *, const mulib_microchan_message_t *);
/** Receive one ack/event. Borrowed payload lasts until next receive/close.
 * Timeout preserves partial framing for a later call; poll timeout=0 is allowed. */
int mulib_microchan_receive (mulib_microchan_client_t *, unsigned timeout_ms,
                            mulib_microchan_message_t *);
/** Nonblocking, single-threaded broker. Zero options: 1MiB payload, 4MiB queue
 * per client, 64 clients, 32 exact-topic subscriptions/client. Prefix is an
 * application namespace, not an identity/authentication mechanism. */
mulib_microchan_broker_t *mulib_microchan_broker_create (const mulib_microchan_options_t *);
/** Own a new mode-0600 socket path. Never removes preexisting filesystem entries. */
int mulib_microchan_broker_listen (mulib_microchan_broker_t *, const char *path);
/** One bounded poll/dispatch round. Bad/disconnected/slow clients are isolated.
 * Publish fanout is all-or-none into bounded queues; ACK means queued, not
 * persisted or processed. Backpressure returns ELIMIT in publisher's ACK. */
int mulib_microchan_broker_step (mulib_microchan_broker_t *, unsigned timeout_ms);
size_t mulib_microchan_broker_clients (const mulib_microchan_broker_t *);
/** Close clients/listener and remove only the socket path created by this broker. */
void mulib_microchan_broker_destroy (mulib_microchan_broker_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MICROCHAN) && !defined(MULIB_MICROCHAN_IMPLEMENTED)
#define MULIB_MICROCHAN_IMPLEMENTED
#include <limits.h>
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#define MULIB_MICROCHAN_POSIX_ 1
#endif
static size_t mulib_microchan_max_ (size_t n) { return n ? n : 1048576; }
static int mulib_microchan_topic_ (const char *s, size_t n)
{
  if (!n || n >= MULIB_MICROCHAN_TOPIC_CAP) return 0;
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '_' || c == '-' || c == '.' || c == '/')) return 0;
  }
  return 1;
}
static int mulib_microchan_valid_ (const mulib_microchan_message_t *m, size_t *topic)
{
  if (!m || !m->id || (m->response != 0 && m->response != 1)
      || m->opcode < MULIB_MICROCHAN_SUBSCRIBE || m->opcode > MULIB_MICROCHAN_EVENT
      || m->status > 0 || m->status < MULIB_CT_ECHECKSUM || (!m->response && m->status)
      || (!m->data && m->length) || mulib_ct_text_length (m->topic, 127, topic)
      || !mulib_microchan_topic_ (m->topic, *topic)) return 0;
  if (m->opcode == MULIB_MICROCHAN_EVENT) return m->response && !m->status;
  return (!m->response && m->opcode == MULIB_MICROCHAN_PUBLISH) || !m->length;
}
int mulib_microchan_encode (const mulib_microchan_message_t *m, size_t max, mulib_ct_blob *out)
{
  mulib_ct_blob payload = {0};
  mulib_container_ipc_message frame;
  size_t topic;
  int rc;
  max = mulib_microchan_max_ (max);
  if (!out || !mulib_microchan_valid_ (m, &topic)) return MULIB_CT_EINVAL;
  if (max > 16777216 || m->length > max) return MULIB_CT_ELIMIT;
  rc = mulib_ct_blob_reserve (&payload, m->length + topic + 2, max + 129);
  if (rc) return rc;
  payload.data[0] = 1; payload.data[1] = (unsigned char)topic;
  memcpy (payload.data + 2, m->topic, topic);
  if (m->length) memcpy (payload.data + 2 + topic, m->data, m->length);
  memset (&frame, 0, sizeof frame);
  frame.opcode = (uint16_t)m->opcode; frame.flags = (uint16_t)m->response;
  frame.status = m->status; frame.request_id = m->id;
  frame.data = payload.data; frame.length = m->length + topic + 2;
  rc = mulib_container_ipc_encode (&frame, out, max + 129);
  mulib_ct_blob_free (&payload); return rc;
}
int mulib_microchan_decode (const void *p, size_t n, size_t max,
    mulib_microchan_message_t *out, size_t *used)
{
  mulib_container_ipc_message frame;
  mulib_microchan_message_t m;
  size_t count, topic;
  int rc;
  if (used) *used = 0;
  max = mulib_microchan_max_ (max);
  if (!out || !used || max > 16777216) return MULIB_CT_EINVAL;
  rc = mulib_container_ipc_decode (p, n, max + 129, &frame, &count);
  if (rc) return rc;
  if (frame.length < 3 || frame.data[0] != 1) return MULIB_CT_EFORMAT;
  topic = frame.data[1];
  if (topic >= 128 || topic + 2 > frame.length || !mulib_microchan_topic_ ((const char *)frame.data + 2, topic))
    return MULIB_CT_EFORMAT;
  memset (&m, 0, sizeof m);
  m.opcode = frame.opcode; m.response = frame.flags; m.status = frame.status; m.id = frame.request_id;
  memcpy (m.topic, frame.data + 2, topic);
  m.data = frame.data + topic + 2; m.length = frame.length - topic - 2;
  if (m.length > max) return MULIB_CT_ELIMIT;
  if (!mulib_microchan_valid_ (&m, &topic)) return MULIB_CT_EFORMAT;
  *out = m; *used = count; return 0;
}
struct mulib_microchan_client {
  mulib_socket_t socket;
  mulib_container_ipc_reader reader;
  size_t max;
  unsigned timeout;
};
int mulib_microchan_open (mulib_microchan_client_t **out, const char *path, size_t max, unsigned timeout)
{
  mulib_microchan_client_t *c;
  int rc;
  max = mulib_microchan_max_ (max);
  if (!out || *out || !path || max > 16777216) return MULIB_CT_EINVAL;
  c = (mulib_microchan_client_t *)calloc (1, sizeof *c);
  if (!c) return MULIB_CT_ENOMEM;
  c->socket.fd = -1; c->max = max; c->timeout = timeout ? timeout : 30000;
  (void)mulib_container_ipc_reader_init (&c->reader, max + 129);
  rc = mulib_socket_local_connect (&c->socket, path, c->timeout);
  if (rc) { free (c); return rc == MULIB_SOCKET_ENOTSUP ? MULIB_CT_ENOTSUP : MULIB_CT_EIO; }
  *out = c; return 0;
}
void mulib_microchan_close (mulib_microchan_client_t *c)
{
  if (c) { mulib_socket_close (&c->socket); mulib_container_ipc_reader_free (&c->reader); free (c); }
}
int mulib_microchan_send (mulib_microchan_client_t *c, const mulib_microchan_message_t *m)
{
  mulib_ct_blob wire = {0};
  size_t off = 0, n;
  int rc;
  if (!c || c->socket.fd < 0) return MULIB_CT_ESTATE;
  rc = mulib_microchan_encode (m, c->max, &wire);
  if (rc) return rc;
  while (off < wire.length) {
    rc = mulib_socket_wait (&c->socket, 1, c->timeout);
    if (rc) break;
    rc = mulib_socket_send (&c->socket, wire.data + off, wire.length - off, &n);
    if (rc == MULIB_SOCKET_AGAIN) continue;
    if (rc || !n) { rc = MULIB_SOCKET_EIO; break; }
    off += n;
  }
  mulib_ct_blob_free (&wire);
  if (rc) { mulib_socket_close (&c->socket); return MULIB_CT_EIO; }
  return 0;
}
/* Read exactly one IPC frame: no loss when multiple frames share a stream. */
static int mulib_microchan_read_ (mulib_socket_t *s, mulib_container_ipc_reader *r)
{
  unsigned char bytes[8192];
  size_t n, used, need = r->expected - r->bytes.length;
  int rc;
  if (r->ready) return MULIB_CT_DONE;
  if (r->error) return r->error;
  if (need > sizeof bytes) need = sizeof bytes;
  rc = mulib_socket_receive (s, bytes, need, &n);
  if (rc == MULIB_SOCKET_AGAIN) return MULIB_CT_AGAIN;
  if (rc || !n) return MULIB_CT_EIO;
  rc = mulib_container_ipc_feed (r, bytes, n, &used);
  return rc;
}
int mulib_microchan_receive (mulib_microchan_client_t *c, unsigned timeout, mulib_microchan_message_t *out)
{
  int rc;
  size_t used;
  mulib_microchan_message_t m;
  if (!c || !out || c->socket.fd < 0) return MULIB_CT_EINVAL;
  if (c->reader.ready) mulib_container_ipc_reset (&c->reader);
  for (;;) {
    rc = mulib_socket_wait (&c->socket, 0, timeout);
    if (rc == MULIB_SOCKET_AGAIN) return MULIB_CT_AGAIN;
    if (rc) { mulib_socket_close (&c->socket); return MULIB_CT_EIO; }
    rc = mulib_microchan_read_ (&c->socket, &c->reader);
    if (rc == MULIB_CT_DONE) {
      rc = mulib_microchan_decode (c->reader.bytes.data, c->reader.bytes.length, c->max, &m, &used);
      if (!rc && !m.response) rc = MULIB_CT_EFORMAT;
      if (!rc) { *out = m; return 0; }
    }
    if (rc != MULIB_CT_AGAIN) { mulib_socket_close (&c->socket); return rc; }
  }
}
typedef struct mulib_microchan_peer_ {
  mulib_socket_t socket;
  mulib_container_ipc_reader reader;
  mulib_ct_blob queue;
  size_t sent, subscriptions;
  char (*topics)[128];
} mulib_microchan_peer_;
struct mulib_microchan_broker {
  mulib_microchan_options_t options;
  char prefix[128], *path;
  mulib_socket_t listener;
  mulib_microchan_peer_ *peers;
#ifdef MULIB_MICROCHAN_POSIX_
  struct pollfd *polls;
  dev_t device;
  ino_t inode;
#endif
};
static void mulib_microchan_peer_free_ (mulib_microchan_peer_ *p)
{
  mulib_socket_close (&p->socket); mulib_container_ipc_reader_free (&p->reader);
  mulib_ct_blob_free (&p->queue); free (p->topics);
  memset (p, 0, sizeof *p); p->socket.fd = -1;
}
mulib_microchan_broker_t *mulib_microchan_broker_create (const mulib_microchan_options_t *options)
{
  mulib_microchan_options_t o = {0};
  mulib_microchan_broker_t *b;
  size_t prefix = 0;
  if (options) o = *options;
  if (!o.max_payload) o.max_payload = 1048576;
  if (!o.max_queue_bytes) o.max_queue_bytes = 4194304;
  if (!o.max_clients) o.max_clients = 64;
  if (!o.max_subscriptions) o.max_subscriptions = 32;
  if (o.max_payload > 16777216 || o.max_queue_bytes < o.max_payload + 161
      || o.max_queue_bytes > 67108864 || o.max_clients > 1024 || o.max_subscriptions > 1024
      || (o.topic_prefix && (mulib_ct_text_length (o.topic_prefix, 127, &prefix)
          || (prefix && !mulib_microchan_topic_ (o.topic_prefix, prefix))))) return NULL;
  b = (mulib_microchan_broker_t *)calloc (1, sizeof *b);
  if (!b) return NULL;
  b->listener.fd = -1; b->options = o;
  if (prefix) memcpy (b->prefix, o.topic_prefix, prefix);
  b->options.topic_prefix = b->prefix;
  b->peers = (mulib_microchan_peer_ *)calloc (o.max_clients, sizeof (*b->peers));
  if (!b->peers) { free (b); return NULL; }
  for (size_t i = 0; i < o.max_clients; ++i) b->peers[i].socket.fd = -1;
#ifdef MULIB_MICROCHAN_POSIX_
  b->polls = (struct pollfd *)calloc (o.max_clients + 1, sizeof (*b->polls));
  if (!b->polls) { mulib_microchan_broker_destroy (b); return NULL; }
#endif
  return b;
}
int mulib_microchan_broker_listen (mulib_microchan_broker_t *b, const char *path)
{
  if (!b || b->listener.fd >= 0 || !path || !*path) return MULIB_CT_EINVAL;
#ifdef MULIB_MICROCHAN_POSIX_
  {
    struct stat st;
    size_t n = strlen (path);
    char *copy = (char *)malloc (n + 1);
    if (!copy) return MULIB_CT_ENOMEM;
    memcpy (copy, path, n + 1);
    if (mulib_socket_local_listen (&b->listener, path)) { free (copy); return MULIB_CT_EIO; }
    if (stat (path, &st) || chmod (path, 0600)) {
      mulib_socket_close (&b->listener); unlink (path); free (copy); return MULIB_CT_EIO;
    }
    b->path = copy; b->device = st.st_dev; b->inode = st.st_ino; return 0;
  }
#else
  return MULIB_CT_ENOTSUP;
#endif
}
void mulib_microchan_broker_destroy (mulib_microchan_broker_t *b)
{
  if (!b) return;
  for (size_t i = 0; i < b->options.max_clients; ++i) mulib_microchan_peer_free_ (b->peers + i);
  free (b->peers); mulib_socket_close (&b->listener);
#ifdef MULIB_MICROCHAN_POSIX_
  if (b->path) {
    struct stat st;
    if (!stat (b->path, &st) && st.st_dev == b->device && st.st_ino == b->inode) (void)unlink (b->path);
  }
  free (b->polls);
#endif
  free (b->path); free (b);
}
size_t mulib_microchan_broker_clients (const mulib_microchan_broker_t *b)
{
  size_t n = 0;
  if (b) for (size_t i = 0; i < b->options.max_clients; ++i) n += b->peers[i].socket.fd >= 0;
  return n;
}
#ifdef MULIB_MICROCHAN_POSIX_
static int mulib_microchan_has_ (const mulib_microchan_peer_ *p, const char *topic)
{
  for (size_t i = 0; i < p->subscriptions; ++i) if (!strcmp (p->topics[i], topic)) return 1;
  return 0;
}
static int mulib_microchan_reserve_ (mulib_microchan_peer_ *p, size_t n, size_t limit)
{
  size_t pending = p->queue.length - p->sent;
  if (n > limit - pending) return MULIB_CT_ELIMIT;
  if (p->sent) { memmove (p->queue.data, p->queue.data + p->sent, pending); p->sent = 0; p->queue.length = pending; }
  return mulib_ct_blob_reserve (&p->queue, pending + n, limit);
}
static void mulib_microchan_queue_ (mulib_microchan_peer_ *p, const mulib_ct_blob *wire)
{
  memcpy (p->queue.data + p->queue.length, wire->data, wire->length); p->queue.length += wire->length;
}
static int mulib_microchan_dispatch_ (mulib_microchan_broker_t *b, mulib_microchan_peer_ *sender,
    const mulib_microchan_message_t *request)
{
  mulib_microchan_message_t ack = *request, event = *request;
  mulib_ct_blob aw = {0}, ew = {0};
  int status = 0, rc;
  size_t ack_size = 34 + strlen (request->topic), limit = b->options.max_queue_bytes;
  if (request->response || request->opcode == MULIB_MICROCHAN_EVENT) return MULIB_CT_EFORMAT;
  /* Reserve acknowledgement before any externally visible operation. */
  rc = mulib_microchan_reserve_ (sender, ack_size, limit);
  if (rc) return rc;
  if (strncmp (request->topic, b->prefix, strlen (b->prefix))) status = MULIB_CT_EACCESS;
  else if (request->opcode == MULIB_MICROCHAN_SUBSCRIBE) {
    if (!mulib_microchan_has_ (sender, request->topic)) {
      if (sender->subscriptions == b->options.max_subscriptions) status = MULIB_CT_ELIMIT;
      /* Commit subscription only after ACK encoding below. */
    }
  } else if (request->opcode == MULIB_MICROCHAN_PUBLISH) {
    event.opcode = MULIB_MICROCHAN_EVENT; event.response = 1;
    status = mulib_microchan_encode (&event, b->options.max_payload, &ew);
    for (size_t i = 0; !status && i < b->options.max_clients; ++i) {
      mulib_microchan_peer_ *p = b->peers + i;
      if (p->socket.fd >= 0 && mulib_microchan_has_ (p, request->topic))
        status = mulib_microchan_reserve_ (p, ew.length + (p == sender ? ack_size : 0), limit);
    }
  }
  ack.response = 1; ack.status = status; ack.data = NULL; ack.length = 0;
  rc = mulib_microchan_encode (&ack, b->options.max_payload, &aw);
  if (rc) { mulib_ct_blob_free (&ew); return rc; }
  mulib_microchan_queue_ (sender, &aw);
  if (!status) {
    if (request->opcode == MULIB_MICROCHAN_SUBSCRIBE && !mulib_microchan_has_ (sender, request->topic))
      memcpy (sender->topics[sender->subscriptions++], request->topic, strlen (request->topic) + 1);
    else if (request->opcode == MULIB_MICROCHAN_UNSUBSCRIBE) {
      for (size_t i = 0; i < sender->subscriptions; ++i) if (!strcmp (sender->topics[i], request->topic)) {
        --sender->subscriptions;
        memcpy (sender->topics[i], sender->topics[sender->subscriptions], 128); break;
      }
    } else if (request->opcode == MULIB_MICROCHAN_PUBLISH) {
      for (size_t i = 0; i < b->options.max_clients; ++i) {
        mulib_microchan_peer_ *p = b->peers + i;
        if (p->socket.fd >= 0 && mulib_microchan_has_ (p, request->topic)) mulib_microchan_queue_ (p, &ew);
      }
    }
  }
  mulib_ct_blob_free (&aw); mulib_ct_blob_free (&ew); return 0;
}
#endif
int mulib_microchan_broker_step (mulib_microchan_broker_t *b, unsigned timeout)
{
  if (!b || b->listener.fd < 0) return MULIB_CT_ESTATE;
#ifdef MULIB_MICROCHAN_POSIX_
  {
    int rc;
    b->polls[0].fd = b->listener.fd; b->polls[0].events = POLLIN; b->polls[0].revents = 0;
    for (size_t i = 0; i < b->options.max_clients; ++i) {
      mulib_microchan_peer_ *p = b->peers + i;
      b->polls[i + 1].fd = p->socket.fd; b->polls[i + 1].events = POLLIN;
      if (p->queue.length > p->sent) b->polls[i + 1].events |= POLLOUT;
      b->polls[i + 1].revents = 0;
    }
    rc = poll (b->polls, (nfds_t)(b->options.max_clients + 1), timeout > INT_MAX ? INT_MAX : (int)timeout);
    if (rc < 0) return errno == EINTR ? 0 : MULIB_CT_EIO;
    if (b->polls[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return MULIB_CT_EIO;
    /* At most one accept and one read/write per peer per round. */
    if (b->polls[0].revents & POLLIN) {
      mulib_socket_t s = MULIB_SOCKET_INIT;
      if (!mulib_socket_accept (&b->listener, &s)) {
        size_t i;
        for (i = 0; i < b->options.max_clients && b->peers[i].socket.fd >= 0; ++i) {}
        if (i == b->options.max_clients || mulib_socket_nonblocking (&s, 1)) mulib_socket_close (&s);
        else {
          mulib_microchan_peer_ *p = b->peers + i;
          p->topics = (char (*)[128])calloc (b->options.max_subscriptions, 128);
          if (!p->topics) mulib_socket_close (&s);
          else { p->socket = s; (void)mulib_container_ipc_reader_init (&p->reader, b->options.max_payload + 129); }
        }
      }
    }
    for (size_t i = 0; i < b->options.max_clients; ++i) {
      mulib_microchan_peer_ *p = b->peers + i;
      short events = b->polls[i + 1].revents;
      if (p->socket.fd < 0) continue;
      if (events & (POLLERR | POLLNVAL)) { mulib_microchan_peer_free_ (p); continue; }
      if (events & (POLLIN | POLLHUP)) {
        rc = mulib_microchan_read_ (&p->socket, &p->reader);
        if (rc == MULIB_CT_DONE) {
          mulib_microchan_message_t m;
          size_t used;
          rc = mulib_microchan_decode (p->reader.bytes.data, p->reader.bytes.length, b->options.max_payload, &m, &used);
          if (!rc) rc = mulib_microchan_dispatch_ (b, p, &m);
          mulib_container_ipc_reset (&p->reader);
        }
        if (rc < 0) { mulib_microchan_peer_free_ (p); continue; }
      }
      if ((events & POLLOUT) && p->queue.length > p->sent) {
        size_t n;
        rc = mulib_socket_send (&p->socket, p->queue.data + p->sent, p->queue.length - p->sent, &n);
        if (rc < 0) { mulib_microchan_peer_free_ (p); continue; }
        p->sent += n;
        if (p->sent == p->queue.length) p->sent = p->queue.length = 0;
      }
    }
    return 0;
  }
#else
  (void)timeout; return MULIB_CT_ENOTSUP;
#endif
}
#undef MULIB_MICROCHAN_POSIX_
#endif
