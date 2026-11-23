#include <assert.h>
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_QUIC
#define MULIB_IMPL_HTTP3
#include "mulib-http3.h"

static void
quic_frames (void)
{
  const unsigned char implicit[] = { 0x0d, 4, 7, 'x', 'y' };
  const unsigned char invalid[] = { 2, 2, 0, 1, 1, 0, 0 };
  const unsigned char expected[] = { 3, 20, 2, 1, 2, 3, 6, 4, 5, 6 };
  const mulib_quic_ack_range_t ranges[] = { { 18, 20 }, { 7, 13 } };
  const mulib_quic_ack_t ack = { 2, 4, 5, 6, 2, 1 };
  mulib_quic_ack_range_t parsed[2] = { { 88, 99 }, { 77, 88 } };
  mulib_quic_ack_t decoded = { 99, 0, 0, 0, 0, 0 };
  mulib_quic_stream_t stream;
  mulib_buffer_t wire = { 0 };
  size_t used = 99;
  assert (mulib_quic_stream_read (implicit, sizeof implicit, &stream, &used) == -1);
  assert (mulib_quic_packet_stream_read (implicit, sizeof implicit, &stream, &used) == 1);
  assert (used == sizeof implicit && stream.offset == 7 && stream.fin
          && stream.length == 2 && !memcmp (stream.data, "xy", 2));
  assert (mulib_quic_ack_write (&wire, &ack, ranges));
  assert (wire.length == sizeof expected && !memcmp (wire.data, expected, wire.length));
  used = 99;
  for (size_t i = 0; i < wire.length; ++i)
    {
      assert (mulib_quic_ack_read (wire.data, i, &decoded, parsed, 2, &used) == 0);
      assert (used == 99 && decoded.delay == 99 && parsed[0].smallest == 88);
    }
  assert (mulib_quic_ack_read (wire.data, wire.length, &decoded, parsed, 1, &used) == -2);
  assert (mulib_quic_ack_read (invalid, sizeof invalid, &decoded, parsed, 2, &used) == -1);
  assert (used == 99 && decoded.delay == 99 && parsed[0].smallest == 88);
  assert (mulib_quic_ack_read (wire.data, wire.length, &decoded, parsed, 2, &used) == 1);
  assert (used == wire.length && decoded.ecn && decoded.delay == 2 && decoded.ce == 6);
  assert (decoded.range_count == 2 && parsed[0].smallest == 18 && parsed[1].largest == 13);
  parsed[1].largest = 17;
  assert (!mulib_quic_ack_write (&wire, &ack, parsed) && wire.length == sizeof expected);
  mulib_buffer_free (&wire);
}

static void
http3_streams (void)
{
  const unsigned char settings[] = { 4, 0 };
  const unsigned char headers[] = { 1, 2, 0, 0 };
  const unsigned char data[] = { 0, 1, 'x' };
  const unsigned char extension[] = { 0x21, 0 };
  const unsigned char goaway[] = { 7, 1, 8 };
  const unsigned char later[] = { 7, 1, 12 };
  mulib_http3_stream_state_t s;
  mulib_http3_frame_t frame;
  size_t used = 99;
  mulib_http3_stream_init (&s, 1, 1);
  assert (mulib_http3_stream_next (&s, extension, sizeof extension, 4, &frame, &used) == -1);
  assert (s.error == MULIB_HTTP3_MISSING_SETTINGS && used == 99);
  mulib_http3_stream_init (&s, 1, 1);
  assert (mulib_http3_stream_next (&s, settings, sizeof settings, 4, &frame, &used) == 1);
  assert (mulib_http3_stream_next (&s, goaway, sizeof goaway, 4, &frame, &used) == 1);
  assert (mulib_http3_stream_next (&s, later, sizeof later, 4, &frame, &used) == -1);
  assert (s.error == MULIB_HTTP3_ID_ERROR);
  mulib_http3_stream_init (&s, 1, 0);
  assert (mulib_http3_stream_finish (&s, 0) == -1);
  assert (s.error == MULIB_HTTP3_CLOSED_CRITICAL_STREAM);

  mulib_http3_stream_init (&s, 0, 0);
  assert (mulib_http3_stream_next (&s, headers, 3, 4, &frame, &used) == 0);
  assert (mulib_http3_stream_next (&s, headers, sizeof headers, 4, &frame, &used) == 1);
  assert (mulib_http3_stream_next (&s, data, sizeof data, 4, &frame, &used) == 1);
  assert (mulib_http3_stream_next (&s, headers, sizeof headers, 4, &frame, &used) == 1);
  assert (mulib_http3_stream_next (&s, data, sizeof data, 4, &frame, &used) == -1);
  assert (s.error == MULIB_HTTP3_FRAME_UNEXPECTED);
  mulib_http3_stream_init (&s, 0, 0);
  assert (mulib_http3_stream_next (&s, headers, sizeof headers, 4, &frame, &used) == 1);
  assert (mulib_http3_stream_finish (&s, 0) == 1);
}

static void
qpack_literals (void)
{
  const unsigned char expected[] = { 0, 0, 0x31, 'x', 1, 'y' };
  const unsigned char indexed[] = { 0, 0, 0xc0 };
  const unsigned char malformed[] = { 0, 0, 0x31, 'x', 1, 'y', 0x27, 128 };
  unsigned char large[300];
  const mulib_http3_field_t literal = { (const unsigned char *)"x",
                                        (const unsigned char *)"y", 1, 1, 1 };
  mulib_http3_field_t long_field = { large, large, sizeof large, sizeof large, 0 };
  mulib_http3_field_t parsed = { NULL, NULL, 42, 11, 0 };
  mulib_buffer_t wire = { 0 };
  size_t count = 42;
  assert (mulib_http3_qpack_literal_write (&wire, &literal, 1));
  assert (wire.length == sizeof expected && !memcmp (wire.data, expected, wire.length));
  assert (mulib_http3_qpack_literal_read (wire.data, wire.length, NULL, 0, &count) == -2);
  assert (mulib_http3_qpack_literal_read (indexed, sizeof indexed, &parsed, 1, &count) == -4);
  assert (mulib_http3_qpack_literal_read (malformed, sizeof malformed, &parsed, 1, &count) == -1);
  assert (parsed.name_length == 42 && count == 42);
  assert (mulib_http3_qpack_literal_read (wire.data, wire.length, &parsed, 1, &count) == 1);
  assert (count == 1 && parsed.never_index && parsed.name[0] == 'x' && parsed.value[0] == 'y');
  wire.length = 0;
  memset (large, 'a', sizeof large);
  assert (mulib_http3_qpack_literal_write (&wire, &long_field, 1));
  assert (mulib_http3_qpack_literal_read (wire.data, wire.length, &parsed, 1, &count) == 1);
  assert (parsed.name_length == sizeof large && parsed.value_length == sizeof large);
  assert (!memcmp (parsed.name, large, sizeof large) && !memcmp (parsed.value, large, sizeof large));
  mulib_buffer_free (&wire);
}

int
main (void)
{
  quic_frames ();
  http3_streams ();
  qpack_literals ();
  return 0;
}
