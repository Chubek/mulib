/** @file mulib-mumq-publisher.h
 * @brief MuMQ exact-topic publisher convenience API.
 * Define MULIB_IMPL_MUMQ_PUBLISHER plus MUMQ dependencies once.
 */
#ifndef MULIB_MUMQ_PUBLISHER_H
#define MULIB_MUMQ_PUBLISHER_H
#include "mulib-mumq.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Copy a publication to a borrowed channel's bounded output queue. */
int mulib_mumq_publish(mulib_mumq_t *, const char *topic, const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUMQ_PUBLISHER) && !defined(MULIB_MUMQ_PUBLISHER_IMPLEMENTED)
#define MULIB_MUMQ_PUBLISHER_IMPLEMENTED
int mulib_mumq_publish(mulib_mumq_t *q, const char *t, const void *p, size_t n) {
  return mulib_mumq_send(q,MULIB_MUMQ_PUBLISH,t,p,n);
}
#endif
