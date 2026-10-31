#include "../container-stack-common.h"
int main (void)
{
  mulib_container_ipc_message message = { 1, 0, 42, 0, (const unsigned char *)"hello", 5 }, parsed;
  mulib_container_ipc_reader reader;
  mulib_ct_blob wire = { 0 };
  size_t i, consumed;
  assert (!mulib_container_ipc_encode (&message, &wire, 1024));
  assert (!mulib_container_ipc_reader_init (&reader, 1024));
  for (i = 0; i < wire.length; ++i)
    assert (mulib_container_ipc_feed (&reader, wire.data + i, 1, &consumed)
        == (i + 1 == wire.length ? MULIB_CT_DONE : MULIB_CT_AGAIN));
  assert (!mulib_container_ipc_peek (&reader, &parsed) && parsed.request_id == 42);
  mulib_container_ipc_reader_free (&reader); mulib_ct_blob_free (&wire);
  return 0;
}
