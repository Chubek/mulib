/* Native HTTP/3 GET: no external QUIC/TLS dependency. */
#include "tls-setup.h"
#define MULIB_IMPL_QUIC
#define MULIB_IMPL_QUIC_CLIENT
#define MULIB_IMPL_HTTP3
#define MULIB_IMPL_HTTP3_CLIENT
#include "mulib-http3.h"

int
main (int argc, char **argv)
{
  mulib_quic_client_options_t options = { 0 };
  mulib_buffer_t body = { 0 };
  unsigned status;
  int rc;
  if (argc < 2 || argc > 3)
    {
      fprintf (stderr, "Usage: %s https://HOST[:PORT]/path [CA.pem]\n", argv[0]);
      return 2;
    }
  if (argc == 3)
    options.ca_file = argv[2];
  rc = mulib_http3_get (argv[1], &options, &status, &body);
  if (rc != MULIB_QUIC_OK)
    fprintf (stderr, "HTTP/3 failed: %d\n", rc);
  else
    {
      fprintf (stderr, "HTTP/3 %u\n", status);
      if (body.length && fwrite (body.data, 1, body.length, stdout) != body.length)
        rc = MULIB_QUIC_IO;
    }
  mulib_buffer_free (&body);
  return rc == MULIB_QUIC_OK ? 0 : 1;
}
