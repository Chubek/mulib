#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_DAEMON
#define MULIB_IMPL_PROCESS
#include "mulib-daemon.h"
#include "mulib-microchan.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif
typedef struct configuration {
  const char *path;
  mulib_microchan_options_t options;
  char prefix[128];
} configuration;
static volatile sig_atomic_t stopping;
static void stop (int signal_number) { (void)signal_number; stopping = 1; }
static int serve (void *user)
{
  configuration *c = (configuration *)user;
  mulib_microchan_broker_t *b;
  int rc;
#if defined(__unix__) || defined(__APPLE__)
  (void)umask (0077);
#endif
  (void)signal (SIGINT, stop); (void)signal (SIGTERM, stop);
  b = mulib_microchan_broker_create (&c->options);
  if (!b) { fputs ("microchand: invalid limits or allocation failure\n", stderr); return 1; }
  rc = mulib_microchan_broker_listen (b, c->path);
  if (rc) fprintf (stderr, "microchand: cannot bind %s (%s)\n", c->path, mulib_ct_strerror (rc));
  while (!rc && !stopping) rc = mulib_microchan_broker_step (b, 100);
  mulib_microchan_broker_destroy (b); return rc ? 1 : 0;
}
static int profile (configuration *c, const char *path)
{
  mulib_mune_state_t *s = mulib_mune_state_create (NULL);
  mulib_mune_result_t *result = NULL;
  mulib_mune_error_t error;
  mulib_mune_value_t table, key, value;
  size_t cursor = 0;
  int ok = 0;
  if (!s) return 0;
  if (mulib_mune_run_file (s, path, &result, &error)) {
    fprintf (stderr, "%s:%zu: %s\n", path, error.offset, error.message); goto done;
  }
  if (mulib_mune_result_count (result) != 1) goto done;
  table = mulib_mune_result_get (result, 0);
  if (table.type != MULIB_MUNE_TABLE) goto done;
  while (mulib_mune_table_next (s, table, &cursor, &key, &value) == 1) {
    size_t n;
    const char *name = mulib_mune_value_string (key, &n);
    size_t *field = NULL;
    if (!name || memchr (name, 0, n)) goto done;
    if (!strcmp (name, "topic_prefix")) {
      const char *text = mulib_mune_value_string (value, &n);
      if (!text || n >= sizeof c->prefix || memchr (text, 0, n)) goto done;
      memcpy (c->prefix, text, n); c->prefix[n] = 0; c->options.topic_prefix = c->prefix; continue;
    }
    if (!strcmp (name, "max_payload")) field = &c->options.max_payload;
    if (!strcmp (name, "max_queue_bytes")) field = &c->options.max_queue_bytes;
    if (!strcmp (name, "max_clients")) field = &c->options.max_clients;
    if (!strcmp (name, "max_subscriptions")) field = &c->options.max_subscriptions;
    if (!field || value.type != MULIB_MUNE_INTEGER || value.as.integer < 0
        || (uint64_t)value.as.integer > SIZE_MAX) goto done;
    *field = (size_t)value.as.integer;
  }
  ok = 1;
done:
  if (!ok) fputs ("microchand: invalid profile\n", stderr);
  mulib_mune_result_destroy (result); mulib_mune_state_destroy (s); return ok;
}
int main (int argc, char **argv)
{
  configuration c = {0};
  const char *profile_path = NULL;
  int background = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp (argv[i], "--socket") && i + 1 < argc) c.path = argv[++i];
    else if (!strcmp (argv[i], "--profile") && i + 1 < argc) profile_path = argv[++i];
    else if (!strcmp (argv[i], "--daemon")) background = 1;
    else {
      fputs ("Usage: microchand --socket /absolute/path [--profile FILE] [--daemon]\n", stderr);
      return !strcmp (argv[i], "--help") ? 0 : 2;
    }
  }
  if (!c.path || c.path[0] != '/') { fputs ("microchand: absolute --socket path required\n", stderr); return 2; }
  if (profile_path && !profile (&c, profile_path)) return 2;
  /* Validate before forking so bad profiles are reported to the invoking shell. */
  {
    mulib_microchan_broker_t *probe = mulib_microchan_broker_create (&c.options);
    if (!probe) { fputs ("microchand: invalid broker limits\n", stderr); return 2; }
    mulib_microchan_broker_destroy (probe);
  }
  if (background) {
    mulib_daemon_t *d = mulib_daemon_create ("microchan");
    mulib_process_t child;
    int rc;
    if (!d) return 1;
    mulib_process_init (&child);
    rc = mulib_daemon_launch (d, serve, &c, &child);
    mulib_daemon_destroy (d);
    if (rc) { fprintf (stderr, "microchand: daemon launch failed (%d)\n", rc); return 1; }
    printf ("%ld\n", child.pid); return 0;
  }
  return serve (&c);
}
