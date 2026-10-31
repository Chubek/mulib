#include "../prowse-common.h"
int main (int argc, char **argv)
{
  mulib_prowse_protocol_t *p;
  mulib_prowse_reply_t reply = { 0 };
  int error;
  if (argc == 1)
    {
      assert (!mulib_prowse_protocol_connect (MULIB_PROWSE_CDP, "file:///bad", NULL, NULL, &error));
      assert (error == MULIB_PROWSE_INVALID); return 0;
    }
  p = mulib_prowse_protocol_connect (MULIB_PROWSE_WEBDRIVER, argv[1], NULL, NULL, &error);
  if (!p) { fprintf (stderr, "%s\n", mulib_prowse_strerror (error)); return 1; }
  error = mulib_prowse_protocol_http (p, "GET", argc > 2 ? argv[2] : "/status", NULL, &reply);
  if (reply.json) { char *text = mulib_json_stringify (reply.json, 1); if (text) { puts (text); free (text); } }
  if (error) fprintf (stderr, "%s\n", mulib_prowse_strerror (error));
  mulib_prowse_reply_free (&reply); mulib_prowse_protocol_destroy (p); return error != 0;
}
