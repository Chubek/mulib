/** @file mulib-mumq-server.h
 * @brief Bounded, backpressured in-memory MuMQ topic broker.
 * Define MULIB_IMPL_MUMQ_SERVER plus MUMQ dependencies once.
 */
#ifndef MULIB_MUMQ_SERVER_H
#define MULIB_MUMQ_SERVER_H
#include "mulib-mumq.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_mumq_server mulib_mumq_server_t;
/** Return nonzero to permit a publish/subscribe by a locally assigned peer
 * slot. Borrowed callback/context; NULL permits all authenticated peers. */
typedef int (*mulib_mumq_authorize_fn)(void *, size_t peer, unsigned type, const char *topic);
/** Allocate a broker for at most 32 peers. No implicit listener/threads. */
mulib_mumq_server_t *mulib_mumq_server_create(mulib_mumq_authorize_fn, void *user);
/** Destroy broker and all owned connections. */
void mulib_mumq_server_destroy(mulib_mumq_server_t *);
/** Transfer channel ownership on success, return 0 or error. */
int mulib_mumq_server_add(mulib_mumq_server_t *, mulib_mumq_t *, size_t *peer);
/** Disconnect a slot; zero on success. Slots can be reused. */
int mulib_mumq_server_remove(mulib_mumq_server_t *, size_t peer);
/** One fair nonblocking pass: flush every peer and read/route at most one
 * message per peer. A full recipient queue retains the publisher's message
 * and routing cursor. No offline delivery/persistence. Faulty peers are
 * disconnected; returns the first error, otherwise zero. */
int mulib_mumq_server_step(mulib_mumq_server_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUMQ_SERVER) && !defined(MULIB_MUMQ_SERVER_IMPLEMENTED)
#define MULIB_MUMQ_SERVER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
typedef struct mulib_mumq_peer_ {
  mulib_mumq_t *channel;
  char topic[256];
  mulib_mumq_message_t pending;
  size_t cursor;
} mulib_mumq_peer_;
struct mulib_mumq_server {
  mulib_mumq_peer_ peers[32];
  mulib_mumq_authorize_fn authorize;
  void *user;
};
mulib_mumq_server_t *mulib_mumq_server_create(mulib_mumq_authorize_fn fn, void *u) {
  mulib_mumq_server_t *s=(mulib_mumq_server_t *)calloc(1,sizeof *s);
  if (s) { s->authorize=fn; s->user=u; } return s;
}
int mulib_mumq_server_remove(mulib_mumq_server_t *s, size_t i) {
  if (!s || i>=32 || !s->peers[i].channel) return -1;
  mulib_mumq_destroy(s->peers[i].channel); mulib_mumq_message_free(&s->peers[i].pending);
  memset(&s->peers[i],0,sizeof s->peers[i]); return 0;
}
void mulib_mumq_server_destroy(mulib_mumq_server_t *s) {
  if (s) { for (size_t i=0;i<32;++i) if (s->peers[i].channel) (void)mulib_mumq_server_remove(s,i); free(s); }
}
int mulib_mumq_server_add(mulib_mumq_server_t *s, mulib_mumq_t *q, size_t *peer) {
  if (!s || !q || !peer) return -1;
  for (size_t i=0;i<32;++i) if (s->peers[i].channel==q) return -1;
  for (size_t i=0;i<32;++i) if (!s->peers[i].channel) {
    s->peers[i].channel=q; *peer=i; return 0;
  }
  return 1;
}
int mulib_mumq_server_step(mulib_mumq_server_t *s) {
  int first=0;
  if (!s) return -1;
  for (size_t i=0;i<32;++i) {
    mulib_mumq_peer_ *p=&s->peers[i]; int rc;
    if (!p->channel) continue;
    rc=mulib_mumq_flush(p->channel);
    if (rc<0) { if (!first) first=rc; (void)mulib_mumq_server_remove(s,i); }
  }
  for (size_t i=0;i<32;++i) {
    mulib_mumq_peer_ *p=&s->peers[i]; int rc=0;
    if (!p->channel) continue;
    if (!p->pending.data) {
      rc=mulib_mumq_receive(p->channel,&p->pending);
      if (rc==1) continue;
      if (rc) goto bad_peer;
      if (p->pending.type==MULIB_MUMQ_SUBACK || (s->authorize
          && !s->authorize(s->user,i,p->pending.type,p->pending.topic))) { rc=-5; goto bad_peer; }
    }
    if (p->pending.type==MULIB_MUMQ_SUBSCRIBE) {
      rc=mulib_mumq_send(p->channel,MULIB_MUMQ_SUBACK,p->pending.topic,NULL,0);
      if (rc==1) continue;
      if (rc) goto bad_peer;
      memcpy(p->topic,p->pending.topic,sizeof p->topic);
    } else {
      while (p->cursor<32) {
        mulib_mumq_peer_ *dst=&s->peers[p->cursor];
        if (dst->channel && !strcmp(dst->topic,p->pending.topic)) {
          rc=mulib_mumq_send(dst->channel,MULIB_MUMQ_PUBLISH,p->pending.topic,p->pending.data,p->pending.length);
          if (rc==1) break;
          if (rc) { if (!first) first=rc; break; }
        }
        ++p->cursor;
      }
      if (p->cursor<32) continue;
    }
    mulib_mumq_message_free(&p->pending); p->cursor=0; continue;
bad_peer:
    if (!first) first=rc;
    (void)mulib_mumq_server_remove(s,i);
  }
  return first;
}
#endif
