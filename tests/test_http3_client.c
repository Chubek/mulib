/* Native implementation TU and optional interoperability test driver. */
#include "../examples/tls-setup.h"
#define MULIB_IMPL_QUIC
#define MULIB_IMPL_QUIC_CLIENT
#define MULIB_IMPL_HTTP3
#define MULIB_IMPL_HTTP3_CLIENT
#include "mulib-http3.h"
#include <assert.h>

static size_t
unhex (const char *hex, unsigned char *out, size_t capacity)
{
  size_t n = strlen (hex) / 2;
  assert (n <= capacity && strlen (hex) == n * 2);
  for (size_t i = 0; i < n; ++i)
    {
      unsigned value;
      assert (sscanf (hex + i * 2, "%2x", &value) == 1);
      out[i] = (unsigned char)value;
    }
  return n;
}

static void
vectors (void)
{
  static const unsigned char cid[] = { 0x83,0x94,0xc8,0xf0,0x3e,0x51,0x57,0x08 };
  static const unsigned char iv[] = { 0xfa,0x04,0x4b,0x2f,0x42,0xa3,0xfd,0x3b,0x46,0xfb,0x25,0x5c };
  static const unsigned char sample[] = { 0xd1,0xb1,0xc9,0x8d,0xd7,0x68,0x9f,0xb8,0xec,0x11,0xd2,0x42,0xb1,0x23,0xdc,0x9b };
  static const unsigned char hp[] = { 0x43,0x7b,0x9a,0xec,0x36 };
  static const unsigned char status[] = { 0, 0, 0xd9 };
  static const unsigned char name_ref[] = { 0, 0, 0xd9, 0x54, 1, '7' };
  static const unsigned char bad_index[] = { 0, 0, 0xff, 36 };
  static const unsigned char nonzero_base[] = { 0, 1, 0xd9 };
  static const unsigned char huffman[] = { 0xf1,0xe3,0xc2,0xe5,0xf2,0x3a,0x6b,0xa0,0xab,0x90,0xf4,0xff };
  static const unsigned char eos[] = { 0xff,0xff,0xff,0xff };
  mulib_http3_response_ response = { 0 };
  mulib_quic_space_ space = { 0 };
  mulib_quic_rx_ stream = { 0 };
  mulib_quic_client_t *client = (mulib_quic_client_t *)calloc (1, sizeof *client);
  unsigned char out[32], wire[256], plain[128];
  uint64_t pn;
  size_t n = 0, header, wire_size, plain_size;
  assert (client);
  assert (mulib_quic_initial_keys_ (&space, cid, sizeof cid));
  assert (!memcmp (space.tx.iv, iv, sizeof iv));
  mulib_aes_gcm_block_ (&space.tx.hp, sample, out);
  assert (!memcmp (out, hp, sizeof hp));
  wire_size = unhex ("cf000000010008f067a5502a4262b5004075c0d95a482cd0991cd25b0aac406a"
                     "5816b6394100f37a1c69797554780bb38cc5a99f5ede4cf73c3ec2493a1839b3"
                     "dbcba3f6ea46c5b7684df3548e7ddeb9c3bf9c73cc3f3bded74b562bfb19fb84"
                     "022f8ef4cdd93795d77d06edbb7aaf2f58891850abbdca3d20398c276456cbc4"
                     "2158407dd074ee", wire, sizeof wire);
  plain_size = unhex ("02000000000600405a020000560303eefce7f7b37ba1d1632e96677825ddf739"
                      "88cfc79825df566dc5430b9a045a1200130100002e00330024001d00209d3c94"
                      "0d89690b84d08a60993c144eca684d1081287c834d5311bcf32bb9da1a002b00"
                      "020304", plain, sizeof plain);
  assert (mulib_quic_packet_open_ (&space, wire, wire_size, 18, &pn, &header) == 1);
  assert (pn == 1 && header == 20 && wire_size == header + plain_size + 16);
  assert (!memcmp (wire + header, plain, plain_size));
  wire_size = unhex ("ff000000010008f067a5502a4262b5746f6b656e04a265ba2eff4d829058fb3f"
                     "0f2496ba", wire, sizeof wire);
  memcpy (client->original, cid, sizeof cid);
  wire[wire_size - 1] ^= 1;
  assert (mulib_quic_retry_ (client, wire, wire_size, 15, wire + 7, 8) == 1 && !client->retry_length);
  wire[wire_size - 1] ^= 1;
  assert (mulib_quic_retry_ (client, wire, wire_size, 15, wire + 7, 8) == 1 && client->retry_length == 5);
  assert (!memcmp (client->retry, "token", 5));
  free (client);
  assert (mulib_quic_packet_number_ (0xa82f31eb, 0x9b32, 2) == 0xa82f9b32);
  assert (mulib_quic_rx_put_ (&stream, 4, (const unsigned char *)"end", 3, 1, 8) == 1);
  assert (!stream.contiguous && stream.final == 7);
  assert (mulib_quic_rx_put_ (&stream, 0, (const unsigned char *)"abcd", 4, 0, 8) == 1);
  assert (stream.contiguous == 7 && !memcmp (stream.data, "abcdend", 7));
  assert (mulib_quic_rx_put_ (&stream, 6, (const unsigned char *)"X", 1, 0, 8) == MULIB_QUIC_PROTOCOL);
  assert (mulib_quic_rx_put_ (&stream, 8, NULL, 0, 1, 8) == MULIB_QUIC_PROTOCOL);
  mulib_quic_rx_free_ (&stream);
  assert (mulib_http3_huffman_decode_ (huffman, sizeof huffman, out, sizeof out, &n) == 1);
  assert (n == 15 && !memcmp (out, "www.example.com", n));
  assert (mulib_http3_huffman_decode_ (eos, sizeof eos, out, sizeof out, &n) == MULIB_QUIC_PROTOCOL);
  assert (mulib_http3_huffman_decode_ (eos, 1, out, sizeof out, &n) == MULIB_QUIC_PROTOCOL);
  assert (mulib_http3_huffman_decode_ (huffman, sizeof huffman, out, 1, &n) == MULIB_QUIC_LIMIT);
  assert (mulib_http3_response_headers_ (&response, bad_index, sizeof bad_index) == MULIB_QUIC_PROTOCOL);
  assert (!response.status);
  assert (mulib_http3_response_headers_ (&response, name_ref, sizeof name_ref) == 1);
  assert (response.status == 200 && response.has_length && response.content_length == 7);
  assert (mulib_http3_response_headers_ (&response, status, sizeof status) == MULIB_QUIC_PROTOCOL);
  memset (&response, 0, sizeof response);
  assert (mulib_http3_response_headers_ (&response, nonzero_base, sizeof nonzero_base) == 1
          && response.status == 200);
}

int
main (int argc, char **argv)
{
  mulib_quic_client_options_t options = { NULL, NULL, NULL, 5000, 0 };
  mulib_buffer_t body = { 0 };
  unsigned status = 999;
  int rc;
  vectors ();
  if (argc == 1)
    return 0;
  assert (argc == 3 || argc == 4);
  options.ca_file = argv[2];
  if (argc == 4)
    options.max_stream_bytes = (size_t)strtoul (argv[3], NULL, 10);
  assert (mulib_buffer_append (&body, "unchanged", 9));
  rc = mulib_http3_get (argv[1], &options, &status, &body);
  if (rc == 1)
    {
      printf ("%u\n", status);
      assert (fwrite (body.data, 1, body.length, stdout) == body.length);
    }
  else
    {
      assert (status == 999 && body.length == 9 && !memcmp (body.data, "unchanged", 9));
      fprintf (stderr, "HTTP/3 error %d\n", rc);
    }
  mulib_buffer_free (&body);
  return rc == 1 ? 0 : 1;
}
