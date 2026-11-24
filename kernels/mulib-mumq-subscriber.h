/** @file mulib-mumq-subscriber.h
 * @brief Acknowledged exact-topic MuMQ subscriptions.
 * Define MULIB_IMPL_MUMQ_SUBSCRIBER plus MUMQ dependencies once.
 */
#ifndef MULIB_MUMQ_SUBSCRIBER_H
#define MULIB_MUMQ_SUBSCRIBER_H
#include "mulib-mumq.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Queue a subscription. Each broker connection has one replaceable topic.
 * Wait for matching SUBACK before relying on delivery. */
int mulib_mumq_subscribe(mulib_mumq_t *, const char *topic);
/** Whether a received message acknowledges this topic. Does not free it. */
int mulib_mumq_subscribed(const mulib_mumq_message_t *, const char *topic);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUMQ_SUBSCRIBER) && !defined(MULIB_MUMQ_SUBSCRIBER_IMPLEMENTED)
#define MULIB_MUMQ_SUBSCRIBER_IMPLEMENTED
#include <string.h>
int mulib_mumq_subscribe(mulib_mumq_t *q, const char *t) {
  return mulib_mumq_send(q,MULIB_MUMQ_SUBSCRIBE,t,NULL,0);
}
int mulib_mumq_subscribed(const mulib_mumq_message_t *m, const char *t) {
  return m && t && m->type==MULIB_MUMQ_SUBACK && !strcmp(m->topic,t);
}
#endif
