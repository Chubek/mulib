#ifndef MULIB_EXAMPLE_MUMQ_COMMON_H
#define MULIB_EXAMPLE_MUMQ_COMMON_H
#define MULIB_IMPL_MUMQ
#define MULIB_IMPL_MUMQ_SERVER
#define MULIB_IMPL_MUMQ_PUBLISHER
#define MULIB_IMPL_MUMQ_SUBSCRIBER
#define MULIB_IMPL_SOCKET
#define MULIB_IMPL_AES
#define MULIB_IMPL_AUTHCRYPTO
#include "mulib-mumq-server.h"
#include "mulib-mumq-publisher.h"
#include "mulib-mumq-subscriber.h"
#include <assert.h>
#include <string.h>
/* One broker, two clients over real nonblocking stream sockets. The same
 * channel API accepts mulib_socket_connect/accept TCP connections. */
static void example_mumq(void) {
  mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT;
  mulib_mumq_t *publisher, *subscriber, *peer;
  mulib_mumq_message_t m={0}; size_t slot;
  mulib_mumq_server_t *server=mulib_mumq_server_create(NULL,NULL);
  assert(server && !mulib_socket_pair(&a,&b));
  publisher=mulib_mumq_create(&a,NULL); peer=mulib_mumq_create(&b,NULL);
  assert(publisher && peer && !mulib_mumq_server_add(server,peer,&slot));
  assert(!mulib_socket_pair(&a,&b));
  subscriber=mulib_mumq_create(&a,NULL); peer=mulib_mumq_create(&b,NULL);
  assert(subscriber && peer && !mulib_mumq_server_add(server,peer,&slot));
  assert(!mulib_mumq_subscribe(subscriber,"example/topic"));
  for (unsigned i=0;i<1000 && !m.data;++i) {
    assert(mulib_mumq_flush(subscriber)>=0); assert(!mulib_mumq_server_step(server));
    assert(mulib_mumq_receive(subscriber,&m)>=0);
  }
  assert(mulib_mumq_subscribed(&m,"example/topic")); mulib_mumq_message_free(&m);
  assert(!mulib_mumq_publish(publisher,"example/topic","hello",5));
  for (unsigned i=0;i<1000 && !m.data;++i) {
    assert(mulib_mumq_flush(publisher)>=0); assert(!mulib_mumq_server_step(server));
    assert(mulib_mumq_receive(subscriber,&m)>=0);
  }
  assert(m.type==MULIB_MUMQ_PUBLISH && m.length==5 && !memcmp(m.data,"hello",5));
  mulib_mumq_message_free(&m); mulib_mumq_destroy(publisher); mulib_mumq_destroy(subscriber);
  mulib_mumq_server_destroy(server);
}
#endif
