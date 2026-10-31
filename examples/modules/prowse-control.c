#include "../prowse-common.h"
int main (int argc, char **argv)
{
  mulib_prowse_control_t *c = mulib_prowse_control_create (NULL, NULL);
  mulib_mune_result_t *result = NULL;
  mulib_mune_error_t error = { MULIB_MUNE_OK, 0, NULL };
  int rc = 0;
  assert (c);
  if (argc == 1)
    {
      const char *source = "local p=require('lprowse'); assert(p.json({ok=true})=='{\"ok\":true}'); assert(not p.status().active)";
      rc = mulib_prowse_control_run (c, source, strlen (source), &result, &error);
    }
  else if (argc == 2 && !strcmp (argv[1], "--repl")) rc = mulib_prowse_control_repl (c, 0, 1);
  else if (argc >= 3)
    {
      mulib_prowse_protocol_kind_t kind = !strcmp (argv[1], "cdp") ? MULIB_PROWSE_CDP
          : !strcmp (argv[1], "marionette") ? MULIB_PROWSE_MARIONETTE : MULIB_PROWSE_WEBDRIVER;
      rc = mulib_prowse_control_connect (c, kind, argv[2], NULL, NULL);
      if (!rc) rc = mulib_prowse_control_start (c, NULL);
      if (!rc && argc > 3)
        {
          mulib_mune_state_t *lua = mulib_prowse_control_lua (c);
          rc = lua ? mulib_mune_run_file (lua, argv[3], &result, &error) : MULIB_MUNE_MEMORY;
        }
      else if (!rc) rc = mulib_prowse_control_repl (c, 0, 1);
      if (rc) fprintf (stderr, "%s\n", error.message ? error.message : mulib_prowse_control_error (c));
      { int cleanup = mulib_prowse_control_quit (c); if (!rc) rc = cleanup; }
    }
  else { fprintf (stderr, "usage: %s [--repl | cdp|webdriver|marionette endpoint [script.lua]]\n", argv[0]); rc = 1; }
  if (rc && argc == 1) fprintf (stderr, "%s\n", error.message ? error.message : mulib_prowse_control_error (c));
  mulib_mune_result_destroy (result); mulib_prowse_control_destroy (c); return rc != 0;
}
