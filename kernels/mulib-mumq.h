/** @file mulib-mumq.h
 * @brief Bounded nonblocking MuMQ v1 topic frames over owned stream sockets.
 * Define MULIB_IMPL_MUMQ plus SOCKET, AES and AUTHCRYPTO once. Not MQTT.
 */
#ifndef MULIB_MUMQ_H
#define MULIB_MUMQ_H
#include "mulib-socket.h"
#include "mulib-aes.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_mumq mulib_mumq_t;
enum { MULIB_MUMQ_OK=0, MULIB_MUMQ_AGAIN=1, MULIB_MUMQ_EOF=2,
       MULIB_MUMQ_INVALID=-1, MULIB_MUMQ_IO=-2, MULIB_MUMQ_MEMORY=-3,
       MULIB_MUMQ_PROTOCOL=-4, MULIB_MUMQ_AUTH=-5 };
enum { MULIB_MUMQ_PUBLISH=1, MULIB_MUMQ_SUBSCRIBE=2, MULIB_MUMQ_SUBACK=3 };
#define MULIB_MUMQ_MAX_PAYLOAD 1048576u
/** Alternative authenticated cipher. Same contracts as AES seal/open; return
 * one on success. user remains borrowed; both callbacks or neither required. */
typedef int (*mulib_mumq_seal_fn)(void *user, const unsigned char nonce[12],
    const void *aad, size_t an, const void *input, size_t n, void *output, unsigned char tag[32]);
typedef int (*mulib_mumq_open_fn)(void *user, const unsigned char nonce[12],
    const void *aad, size_t an, const void *input, size_t n, const unsigned char tag[32], void *output);
typedef struct mulib_mumq_options {
  const unsigned char *key; /**< NULL for plaintext; otherwise copies 64 bytes. */
  unsigned char tx_prefix[4], rx_prefix[4]; /**< Different, externally unique per key/session/direction. */
  mulib_mumq_seal_fn seal;
  mulib_mumq_open_fn open;
  void *user;
} mulib_mumq_options_t;
/** Owned message: initialize {0}; free before reuse. Topic is 1..255 ASCII
 * alnum, dot, underscore, hyphen or slash. Payload is arbitrary bytes. */
typedef struct mulib_mumq_message {
  unsigned type;
  char topic[256];
  unsigned char *data;
  size_t length;
} mulib_mumq_message_t;
/** Move a connected socket on success; enable nonblocking I/O. NULL options
 * explicitly select plaintext. Single-threaded or externally synchronized. */
mulib_mumq_t *mulib_mumq_create(mulib_socket_t *socket, const mulib_mumq_options_t *options);
/** Close the socket, wipe copied keys and free all buffers. NULL safe. */
void mulib_mumq_destroy(mulib_mumq_t *);
/** Borrow fd for a caller-owned event loop; -1 for invalid. */
int mulib_mumq_fd(const mulib_mumq_t *);
/** Validate a NUL-terminated exact-match topic. No wildcards. */
int mulib_mumq_topic(const char *);
/** Copy/queue one frame. AGAIN means no mutation: queue limited to 16 frames
 * and 4 MiB. Subscribe/ack payload must be empty. No implicit flush. */
int mulib_mumq_send(mulib_mumq_t *, unsigned type, const char *topic, const void *data, size_t length);
/** Advance short writes; AGAIN retains buffered bytes. */
int mulib_mumq_flush(mulib_mumq_t *);
/** Read at most one frame, retaining partial reads. Output must have data=NULL.
 * Auth/protocol/I/O/EOF errors poison the connection; destroy/reconnect. */
int mulib_mumq_receive(mulib_mumq_t *, mulib_mumq_message_t *);
/** Release owned payload and reset. NULL safe. */
void mulib_mumq_message_free(mulib_mumq_message_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUMQ) && !defined(MULIB_MUMQ_IMPLEMENTED)
#define MULIB_MUMQ_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
typedef struct mulib_mumq_frame_ {
  struct mulib_mumq_frame_ *next;
  size_t size, at;
  unsigned char bytes[];
} mulib_mumq_frame_;
struct mulib_mumq {
  mulib_socket_t socket;
  mulib_mumq_options_t options;
  unsigned char key[64], header[32], *body;
  size_t header_at, body_at, body_size, queued, frames;
  uint64_t tx, rx;
  int secure, failed;
  mulib_mumq_frame_ *first, *last;
};
static void mulib_mumq_put_(unsigned char *p, uint64_t v, unsigned n) {
  while (n) { p[--n] = (unsigned char)v; v >>= 8; }
}
static uint64_t mulib_mumq_get_(const unsigned char *p, unsigned n) {
  uint64_t v=0; while (n--) v = (v<<8) | *p++; return v;
}
int mulib_mumq_topic(const char *s) {
  size_t n;
  if (!s || !*s) return 0;
  for (n=0; s[n]; ++n) {
    unsigned char c=(unsigned char)s[n];
    if (n==255 || !((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9')
        || c=='/' || c=='_' || c=='-' || c=='.')) return 0;
  }
  return 1;
}
mulib_mumq_t *mulib_mumq_create(mulib_socket_t *s, const mulib_mumq_options_t *o) {
  mulib_mumq_t *q;
  if (!s || s->fd<0 || (o && (!!o->seal != !!o->open))) return NULL;
  if (o && (o->key || o->seal) && !memcmp(o->tx_prefix,o->rx_prefix,4)) return NULL;
  q=(mulib_mumq_t *)calloc(1,sizeof *q); if (!q) return NULL;
  if (mulib_socket_nonblocking(s,1)) { free(q); return NULL; }
  q->socket=*s; s->fd=-1;
  if (o) {
    q->options=*o; q->secure=!!(o->key || o->seal);
    if (o->key) { memcpy(q->key,o->key,64); q->options.key=q->key; }
  }
  return q;
}
void mulib_mumq_destroy(mulib_mumq_t *q) {
  if (!q) return;
  (void)mulib_socket_close(&q->socket); free(q->body);
  while (q->first) { mulib_mumq_frame_ *p=q->first; q->first=p->next; free(p); }
  mulib_authcrypto_wipe(q,sizeof *q); free(q);
}
int mulib_mumq_fd(const mulib_mumq_t *q) { return q ? q->socket.fd : -1; }
void mulib_mumq_message_free(mulib_mumq_message_t *m) {
  if (m) { free(m->data); memset(m,0,sizeof *m); }
}
int mulib_mumq_send(mulib_mumq_t *q, unsigned type, const char *topic, const void *data, size_t n) {
  mulib_mumq_frame_ *f;
  unsigned char *h, *b;
  size_t tn, size;
  if (!q || q->failed || !mulib_mumq_topic(topic) || (!data && n)
      || n>MULIB_MUMQ_MAX_PAYLOAD || type<1 || type>3 || (type!=1 && n)) return -1;
  tn=strlen(topic); size=32+tn+n+(q->secure?32:0);
  if (q->frames==16 || size>4194304-q->queued) return 1;
  if (q->tx==UINT64_MAX) return MULIB_MUMQ_PROTOCOL;
  f=(mulib_mumq_frame_ *)malloc(sizeof *f+size); if (!f) return -3;
  f->next=NULL; f->size=size; f->at=0; h=f->bytes; b=h+32;
  memset(h,0,32); memcpy(h,"MMQ1",4); h[4]=(unsigned char)type; h[5]=(unsigned char)q->secure;
  mulib_mumq_put_(h+6,tn,2); mulib_mumq_put_(h+8,n,4); mulib_mumq_put_(h+12,q->tx+1,8);
  memcpy(b,topic,tn); if (n) memcpy(b+tn,data,n);
  if (q->secure) {
    int ok;
    memcpy(h+20,q->options.tx_prefix,4); memcpy(h+24,h+12,8);
    ok=q->options.seal ? q->options.seal(q->options.user,h+20,h,32,b,tn+n,b,b+tn+n)
      : mulib_aes_seal(q->key,h+20,h,32,b,tn+n,b,b+tn+n);
    if (!ok) { free(f); return MULIB_MUMQ_AUTH; }
  }
  if (q->last) q->last->next=f; else q->first=f;
  q->last=f; ++q->frames; q->queued+=size; ++q->tx; return 0;
}
int mulib_mumq_flush(mulib_mumq_t *q) {
  if (!q || q->failed) return -1;
  while (q->first) {
    mulib_mumq_frame_ *f=q->first;
    size_t n;
    int rc=mulib_socket_send(&q->socket,f->bytes+f->at,f->size-f->at,&n);
    if (rc==1) return 1;
    if (rc || !n) { q->failed=1; return -2; }
    f->at+=n;
    if (f->at<f->size) return 1;
    q->first=f->next; if (!q->first) q->last=NULL;
    q->queued-=f->size; --q->frames; free(f);
  }
  return 0;
}
static int mulib_mumq_fail_(mulib_mumq_t *q, int rc) { q->failed=1; return rc; }
int mulib_mumq_receive(mulib_mumq_t *q, mulib_mumq_message_t *out) {
  size_t n, tn, pn; int rc;
  mulib_mumq_message_t m={0};
  if (!q || !out || out->data || q->failed) return -1;
  if (q->header_at<32) {
    rc=mulib_socket_receive(&q->socket,q->header+q->header_at,32-q->header_at,&n);
    if (rc==1) return 1;
    if (rc) return mulib_mumq_fail_(q,rc==2?2:-2);
    q->header_at+=n; if (q->header_at<32) return 1;
  }
  tn=(size_t)mulib_mumq_get_(q->header+6,2); pn=(size_t)mulib_mumq_get_(q->header+8,4);
  if (!q->body) {
    unsigned char zero[12]={0};
    if (memcmp(q->header,"MMQ1",4) || q->header[4]<1 || q->header[4]>3
        || q->header[5]!=q->secure || !tn || tn>255 || pn>MULIB_MUMQ_MAX_PAYLOAD
        || (q->header[4]!=1 && pn) || q->rx==UINT64_MAX
        || mulib_mumq_get_(q->header+12,8)!=q->rx+1
        || (q->secure ? (memcmp(q->header+20,q->options.rx_prefix,4)
                         || memcmp(q->header+24,q->header+12,8)) : memcmp(q->header+20,zero,12)))
      return mulib_mumq_fail_(q,MULIB_MUMQ_PROTOCOL);
    q->body_size=tn+pn+(q->secure?32:0);
    q->body=(unsigned char *)malloc(q->body_size);
    if (!q->body) return mulib_mumq_fail_(q,-3);
  }
  if (q->body_at<q->body_size) {
    rc=mulib_socket_receive(&q->socket,q->body+q->body_at,q->body_size-q->body_at,&n);
    if (rc==1) return 1;
    if (rc) return mulib_mumq_fail_(q,rc==2?2:-2);
    q->body_at+=n; if (q->body_at<q->body_size) return 1;
  }
  if (q->secure) {
    int ok=q->options.open ? q->options.open(q->options.user,q->header+20,q->header,32,
      q->body,tn+pn,q->body+tn+pn,q->body)
      : mulib_aes_open(q->key,q->header+20,q->header,32,q->body,tn+pn,q->body+tn+pn,q->body);
    if (!ok) return mulib_mumq_fail_(q,MULIB_MUMQ_AUTH);
  }
  memcpy(m.topic,q->body,tn); m.topic[tn]=0;
  if (memchr(m.topic,0,tn) || !mulib_mumq_topic(m.topic)) return mulib_mumq_fail_(q,-4);
  m.data=(unsigned char *)malloc(pn+1);
  if (!m.data) return mulib_mumq_fail_(q,-3);
  memcpy(m.data,q->body+tn,pn); m.data[pn]=0; m.length=pn; m.type=q->header[4];
  free(q->body); q->body=NULL; q->header_at=q->body_at=q->body_size=0; ++q->rx;
  *out=m; return 0;
}
#endif
