/* Separate C11 consumer of the planned QUIC + HTTP/3 archive. */
#include "mulib.h"
#include <assert.h>
#include <stdio.h>

int
main (void)
{
  mulib_buffer_t buffer = { 0 };
  mulib_quic_client_t *client = NULL;
  unsigned status = 731;
  mulib_http3_frame_t settings = { 4, NULL, 0 }, frame;
  size_t n;
  assert (mulib_http3_frame_write (&buffer, &settings));
  assert (mulib_http3_frame_read (buffer.data, buffer.length, 0, &frame, &n) == 1);
  assert (n == buffer.length && frame.type == 4);
  assert (mulib_http3_get ("http://invalid/", NULL, &status, &buffer) == MULIB_QUIC_INVALID);
  assert (status == 731 && buffer.length == 2);
  assert (mulib_quic_client_connect ("localhost", 0, NULL, &client) == MULIB_QUIC_INVALID);
  assert (!client);
  assert (mulib_quic_client_send (client, 0, NULL, 0, 0) == MULIB_QUIC_INVALID);
  assert (mulib_quic_client_poll (client) == MULIB_QUIC_INVALID);
  assert (mulib_quic_client_stream (client, 0, NULL) == MULIB_QUIC_INVALID);
  mulib_quic_client_close (client);
  mulib_buffer_free (&buffer);
  puts ("quic + http3: archive OK");
  return 0;
}
