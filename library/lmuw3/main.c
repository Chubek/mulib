#include "mulib-muw3-mune.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static void usage (void)
{
  fputs ("Usage: muw3 [-X METHOD] [-H 'Name: value'] [-d DATA] [-i] [-o FILE]\n"
         "            [--redirects N] [--timeout MS] [--cacert PEM] URL\n"
         "       muw3 [--cacert PEM] --weblet FILE [ARGS...]\n", stderr);
}
static int number (const char *s, unsigned max, unsigned *out)
{
  char *end;
  unsigned long n;
  if (!s || !*s || *s == '-') return 0;
  errno = 0; n = strtoul (s, &end, 10);
  if (errno || *end || n > max) return 0;
  *out = (unsigned)n; return 1;
}
static int weblet (int argc, char **argv, const mulib_muw3_client_t *client)
{
  mulib_mune_error_t error;
  mulib_muw3_engine_t *e = mulib_muw3_engine_create (client, NULL, &error);
  mulib_mune_result_t *result = NULL, *roots;
  mulib_mune_value_t args, text;
  mulib_mune_state_t *s;
  int rc = 0;
  if (!e) { fprintf (stderr, "%s\n", error.message); return 1; }
  s = mulib_muw3_engine_lua (e); roots = mulib_mune_result_create (s);
  if (!roots || mulib_mune_make_table (s, &args) || !mulib_mune_result_push (roots, args)) rc = 1;
  for (int i = 0; !rc && i < argc; ++i) {
    if (mulib_mune_make_string (s, argv[i], strlen (argv[i]), &text)
        || !mulib_mune_result_push (roots, text)
        || mulib_mune_raw_set (s, args, mulib_mune_integer (i), text)) rc = 1;
  }
  if (!rc) rc = mulib_mune_set_global (s, "arg", args);
  mulib_mune_result_destroy (roots);
  if (!rc) {
    rc = mulib_muw3_engine_run_file (e, argv[0], &result, &error);
    if (rc) fprintf (stderr, "%s:%zu: %s\n", argv[0], error.offset, error.message);
  }
  mulib_mune_result_destroy (result); mulib_muw3_engine_destroy (e); return rc ? 1 : 0;
}
int main (int argc, char **argv)
{
  mulib_muw3_client_t client = mulib_muw3_client_default ();
  mulib_muw3_request_t request = {0};
  mulib_muw3_response_t response = {0};
  mulib_http_header_t headers[96];
  char *owned[96];
  const char *output = NULL;
  int include = 0, rc = 2;
  FILE *f = stdout;
  request.headers = headers;
  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (!strcmp (arg, "--help")) { usage (); rc = 0; goto done; }
    if (!strcmp (arg, "--weblet") && i + 1 < argc) {
      rc = weblet (argc - i - 1, argv + i + 1, &client); goto done;
    }
    if (!strcmp (arg, "-i")) include = 1;
    else if (!strcmp (arg, "--cacert") && i + 1 < argc) client.options.ca_file = argv[++i];
    else if (!strcmp (arg, "-X") && i + 1 < argc) request.method = argv[++i];
    else if (!strcmp (arg, "-d") && i + 1 < argc) {
      request.body = argv[++i]; request.body_length = strlen ((const char *)request.body);
      if (!request.method) request.method = "POST";
    } else if (!strcmp (arg, "-o") && i + 1 < argc) output = argv[++i];
    else if (!strcmp (arg, "--redirects") && i + 1 < argc) {
      if (!number (argv[++i], 100, &client.options.max_redirects)) goto invalid;
    } else if (!strcmp (arg, "--timeout") && i + 1 < argc) {
      if (!number (argv[++i], 3600000, &client.options.timeout_ms) || !client.options.timeout_ms) goto invalid;
    } else if (!strcmp (arg, "-H") && i + 1 < argc && request.header_count < 96) {
      const char *text = argv[++i], *colon = strchr (text, ':');
      char *p;
      size_t index = request.header_count;
      if (!colon || colon == text) goto invalid;
      p = (char *)malloc (strlen (text) + 1);
      if (!p) { rc = 1; goto done; }
      strcpy (p, text); p[colon - text] = 0; owned[index] = p;
      headers[index].name = p; headers[index].value = p + (colon - text) + 1;
      ++request.header_count;
    } else if (*arg != '-' && !request.url) request.url = arg;
    else goto invalid;
  }
  if (!request.url) goto invalid;
  rc = mulib_muw3_perform (&client, &request, &response);
  if (rc != MULIB_MUW3_OK) { fprintf (stderr, "muw3: %s\n", mulib_muw3_strerror (rc)); rc = 1; goto done; }
  rc = 1;
  if (output && !(f = fopen (output, "wb"))) { perror (output); goto done; }
  if (include) {
    if (fprintf (f, "HTTP/1.1 %u %s\r\n", response.message.status, response.message.reason) < 0) goto close;
    for (size_t i = 0; i < response.message.header_count; ++i)
      if (fprintf (f, "%s: %s\r\n", response.message.headers[i].name, response.message.headers[i].value) < 0) goto close;
    if (fputs ("\r\n", f) == EOF) goto close;
  }
  if (response.message.body_length && fwrite (response.message.body, 1, response.message.body_length, f) != response.message.body_length) goto close;
  rc = 0;
close:
  if (f == stdout) { if (fflush (f)) rc = 1; }
  else if (fclose (f)) rc = 1;
  goto done;
invalid:
  usage ();
done:
  for (size_t i = 0; i < request.header_count; ++i) free (owned[i]);
  mulib_muw3_response_free (&response); return rc;
}
