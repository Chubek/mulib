#include "../library/microchan/runtime.c"
#include <assert.h>
#include <stdio.h>
static void codec (void)
{
  mulib_microchan_message_t m = {0}, decoded = {0};
  mulib_ct_blob wire = {0};
  size_t used = 9;
  m.opcode = MULIB_MICROCHAN_PUBLISH; m.id = UINT64_MAX;
  strcpy (m.topic, "muw3/test"); m.data = (const unsigned char *)"a\0b"; m.length = 3;
  assert (!mulib_microchan_encode (&m, 3, &wire));
  for (size_t i = 0; i < wire.length; ++i) {
    decoded.id = 42;
    assert (mulib_microchan_decode (wire.data, i, 3, &decoded, &used) == MULIB_CT_AGAIN);
    assert (!used && decoded.id == 42);
  }
  assert (!mulib_microchan_decode (wire.data, wire.length, 3, &decoded, &used));
  assert (used == wire.length && decoded.id == UINT64_MAX && !strcmp (decoded.topic, m.topic)
      && decoded.length == 3 && !memcmp (decoded.data, "a\0b", 3));
  assert (mulib_microchan_decode (wire.data, wire.length, 2, &decoded, &used) == MULIB_CT_ELIMIT);
  wire.data[wire.length - 1] ^= 1;
  assert (mulib_microchan_decode (wire.data, wire.length, 3, &decoded, &used) == MULIB_CT_ECHECKSUM);
  wire.data[wire.length - 1] ^= 1;
  m.id = 0;
  assert (mulib_microchan_encode (&m, 3, &wire) == MULIB_CT_EINVAL);
  m.id = 1; m.opcode = MULIB_MICROCHAN_SUBSCRIBE;
  assert (mulib_microchan_encode (&m, 3, &wire) == MULIB_CT_EINVAL);
  mulib_ct_blob_free (&wire);
}
#if defined(__unix__) || defined(__APPLE__)
static void pump (mulib_microchan_broker_t *b)
{ for (int i = 0; i < 16; ++i) assert (!mulib_microchan_broker_step (b, 0)); }
static void command (mulib_microchan_broker_t *b, mulib_microchan_client_t *c,
    unsigned op, const char *topic, int status)
{
  mulib_microchan_message_t m = {0}, reply;
  m.opcode = op; m.id = 42; strcpy (m.topic, topic);
  assert (!mulib_microchan_send (c, &m)); pump (b);
  assert (!mulib_microchan_receive (c, 100, &reply));
  assert (reply.response && reply.id == 42 && reply.opcode == op && reply.status == status);
}
static void broker (void)
{
  char directory[] = "/tmp/opencode/microchan-test-XXXXXX", path[256];
  mulib_microchan_options_t options = {64, 1024, 3, 1, "muw3/"};
  mulib_microchan_broker_t *b = mulib_microchan_broker_create (&options), *collision;
  mulib_microchan_client_t *a = NULL, *c = NULL;
  mulib_microchan_message_t m = {0}, reply;
  assert (b && mkdtemp (directory));
  snprintf (path, sizeof path, "%s/bus.sock", directory);
  assert (!mulib_microchan_broker_listen (b, path));
  collision = mulib_microchan_broker_create (NULL);
  assert (collision && mulib_microchan_broker_listen (collision, path) == MULIB_CT_EIO);
  mulib_microchan_broker_destroy (collision);
  assert (!mulib_microchan_open (&a, path, 64, 100));
  assert (!mulib_microchan_open (&c, path, 64, 100)); pump (b);
  assert (mulib_microchan_broker_clients (b) == 2);
  command (b, a, MULIB_MICROCHAN_SUBSCRIBE, "muw3/topic", 0);
  command (b, a, MULIB_MICROCHAN_SUBSCRIBE, "muw3/topic", 0);
  command (b, a, MULIB_MICROCHAN_SUBSCRIBE, "muw3/other", MULIB_CT_ELIMIT);
  command (b, c, MULIB_MICROCHAN_SUBSCRIBE, "other/topic", MULIB_CT_EACCESS);
  command (b, c, MULIB_MICROCHAN_SUBSCRIBE, "muw3/topic", 0);
  m.opcode = MULIB_MICROCHAN_PUBLISH; m.id = 70; strcpy (m.topic, "muw3/topic");
  m.data = (const unsigned char *)"x\0y"; m.length = 3;
  assert (!mulib_microchan_send (a, &m)); pump (b);
  assert (!mulib_microchan_receive (a, 100, &reply) && reply.opcode == MULIB_MICROCHAN_PUBLISH);
  assert (!mulib_microchan_receive (a, 100, &reply) && reply.opcode == MULIB_MICROCHAN_EVENT);
  assert (!mulib_microchan_receive (c, 100, &reply) && reply.opcode == MULIB_MICROCHAN_EVENT
      && reply.id == 70 && reply.length == 3 && !memcmp (reply.data, "x\0y", 3));
  assert (mulib_microchan_receive (c, 0, &reply) == MULIB_CT_AGAIN);
  command (b, c, MULIB_MICROCHAN_UNSUBSCRIBE, "muw3/topic", 0);
  assert (!mulib_microchan_send (a, &m)); pump (b);
  assert (!mulib_microchan_receive (a, 100, &reply));
  assert (!mulib_microchan_receive (a, 100, &reply));
  assert (mulib_microchan_receive (c, 0, &reply) == MULIB_CT_AGAIN);
  /* Invalid CRC from a third client must not disturb existing peers. */
  {
    mulib_socket_t bad = MULIB_SOCKET_INIT;
    unsigned char bytes[32] = {0}; size_t sent;
    assert (!mulib_socket_local_connect (&bad, path, 100)); pump (b);
    assert (!mulib_socket_send (&bad, bytes, sizeof bytes, &sent) && sent == sizeof bytes); pump (b);
    assert (mulib_microchan_broker_clients (b) == 2);
    mulib_socket_close (&bad);
  }
  command (b, c, MULIB_MICROCHAN_SUBSCRIBE, "muw3/alive", 0);
  mulib_microchan_close (a); mulib_microchan_close (c); pump (b);
  assert (!mulib_microchan_broker_clients (b));
  mulib_microchan_broker_destroy (b);
  assert (access (path, F_OK) != 0 && !rmdir (directory));
}
static void backpressure (void)
{
  char directory[] = "/tmp/opencode/microchan-pressure-XXXXXX", path[256];
  mulib_microchan_options_t options = {65536, 65700, 3, 1, NULL};
  mulib_microchan_broker_t *b = mulib_microchan_broker_create (&options);
  mulib_microchan_client_t *publisher = NULL, *fast = NULL, *slow = NULL;
  mulib_microchan_message_t m = {0}, reply;
  unsigned char *payload = (unsigned char *)malloc (65536);
  int rejected = 0;
  assert (b && payload && mkdtemp (directory));
  memset (payload, 0xa5, 65536);
  snprintf (path, sizeof path, "%s/bus.sock", directory);
  assert (!mulib_microchan_broker_listen (b, path));
  assert (!mulib_microchan_open (&publisher, path, 65536, 100)); pump (b);
  assert (!mulib_microchan_open (&fast, path, 65536, 100)); pump (b);
  assert (!mulib_microchan_open (&slow, path, 65536, 100)); pump (b);
  command (b, fast, MULIB_MICROCHAN_SUBSCRIBE, "topic", 0);
  command (b, slow, MULIB_MICROCHAN_SUBSCRIBE, "topic", 0);
  m.opcode = MULIB_MICROCHAN_PUBLISH; strcpy (m.topic, "topic");
  m.data = payload; m.length = 65536;
  for (unsigned i = 1; i <= 64; ++i) {
    m.id = i;
    assert (!mulib_microchan_send (publisher, &m)); pump (b);
    assert (!mulib_microchan_receive (publisher, 100, &reply) && reply.id == i);
    if (reply.status == MULIB_CT_ELIMIT) {
      /* Fast subscriber was reserved first, but must not see a partial fanout. */
      assert (mulib_microchan_receive (fast, 0, &reply) == MULIB_CT_AGAIN);
      rejected = 1; break;
    }
    assert (!reply.status);
    assert (!mulib_microchan_receive (fast, 100, &reply) && reply.id == i && reply.length == 65536);
    assert (!memcmp (reply.data, payload, 65536));
  }
  assert (rejected);
  mulib_microchan_close (slow); pump (b);
  m.id = 100;
  assert (!mulib_microchan_send (publisher, &m)); pump (b);
  assert (!mulib_microchan_receive (publisher, 100, &reply) && !reply.status);
  assert (!mulib_microchan_receive (fast, 100, &reply) && reply.id == 100);
  mulib_microchan_close (publisher); mulib_microchan_close (fast);
  mulib_microchan_broker_destroy (b); free (payload);
  assert (!rmdir (directory));
}
#if defined(__linux__)
static void backlog (void)
{
  char directory[] = "/tmp/opencode/microchan-backlog-XXXXXX", path[256];
  mulib_socket_t listener = MULIB_SOCKET_INIT, clients[64];
  size_t count = 0;
  int rc = 0;
  assert (mkdtemp (directory));
  snprintf (path, sizeof path, "%s/bus.sock", directory);
  assert (!mulib_socket_local_listen (&listener, path));
  for (; count < 64; ++count) {
    clients[count].fd = -1;
    rc = mulib_socket_local_connect (clients + count, path, 10);
    if (rc) { assert (rc == MULIB_SOCKET_AGAIN && clients[count].fd == -1); break; }
  }
  assert (rc == MULIB_SOCKET_AGAIN); /* Never hand out a false-connected fd. */
  for (size_t i = 0; i < count; ++i) mulib_socket_close (clients + i);
  mulib_socket_close (&listener);
  assert (!unlink (path) && !rmdir (directory));
}
#endif
#endif
int main (void)
{
  codec ();
#if defined(__unix__) || defined(__APPLE__)
  broker ();
  backpressure ();
#if defined(__linux__)
  backlog ();
#endif
#endif
  puts ("MicroChan: framing, fanout, acknowledgements, namespace and lifecycle passed");
  return 0;
}
