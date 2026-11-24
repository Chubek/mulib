/**
 * @file mulib-daemon.h
 * @brief Owned daemon specifications, Mune configuration/profiles and POSIX launch.
 * Define MULIB_IMPL_DAEMON and the transitive MUNE/PROCESS switches in the manual.
 */
#ifndef MULIB_DAEMON_H
#define MULIB_DAEMON_H
#include "mulib-mune.h"
#include "mulib-process.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_daemon mulib_daemon_t;
typedef enum mulib_daemon_status {
  MULIB_DAEMON_OK = 0, MULIB_DAEMON_INVALID = -1, MULIB_DAEMON_MEMORY = -2,
  MULIB_DAEMON_SCRIPT = -3, MULIB_DAEMON_SYSTEM = -4, MULIB_DAEMON_UNSUPPORTED = -5
} mulib_daemon_status_t;
typedef struct mulib_daemon_error {
  int code;
  size_t offset;
  char message[256];              /**< Owned text, including after Mune teardown. */
} mulib_daemon_error_t;
/** Child-only entry; return a process exit code (0..255). Opens its own resources. */
typedef int (*mulib_daemon_entry_fn) (void *user);
/** Create a specification: cwd '/', restart 'on-failure', empty environment.
 * Names contain 1..127 ASCII letters, digits, underscores, hyphens or dots. */
mulib_daemon_t *mulib_daemon_create (const char *name);
/** Destroy owned strings/vectors; NULL safe. */
void mulib_daemon_destroy (mulib_daemon_t *);
/** Replace copied description, working directory (absolute), or restart policy
 * ('no', 'on-failure', 'always'). Text is limited to 4095 non-control bytes. */
int mulib_daemon_description (mulib_daemon_t *, const char *);
int mulib_daemon_directory (mulib_daemon_t *, const char *);
int mulib_daemon_restart (mulib_daemon_t *, const char *);
/** Replace copied argv, including absolute executable argv[0]; 1..256 entries. */
int mulib_daemon_command (mulib_daemon_t *, const char *const *argv, size_t argc);
/** Set/replace a copied environment entry; max 256, ASCII identifier keys. */
int mulib_daemon_environment (mulib_daemon_t *, const char *key, const char *value);
/** Borrow the service name. */
const char *mulib_daemon_name (const mulib_daemon_t *);
/** Evaluate a Mune configuration using daemon_name/description/directory/restart,
 * daemon_command(arg0,...) and daemon_environment(key,value). Returns an owned
 * spec only on success; scripts have ordinary Mune host capabilities. */
mulib_daemon_t *mulib_daemon_load (const char *source, size_t length, mulib_daemon_error_t *);
/** Load a configuration file, bounded to 1 MiB. */
mulib_daemon_t *mulib_daemon_load_file (const char *path, mulib_daemon_error_t *);
/** Evaluate a profile with a global 'daemon' specification table. Profile must
 * return one NUL-free string. Append transactionally to an initialized builder.
 * library/ldaemon/systemd.lua produces a .service unit; compilation has no
 * implicit installation or service-manager side effects. */
int mulib_daemon_compile (const mulib_daemon_t *, const char *profile, size_t length,
                         mulib_strbuf_t *out, mulib_daemon_error_t *);
/** Compile a profile file, bounded to 1 MiB. */
int mulib_daemon_compile_file (const mulib_daemon_t *, const char *path,
                              mulib_strbuf_t *out, mulib_daemon_error_t *);
/** Start an owned POSIX child session: setsid, cwd, umask 027, /dev/null stdio,
 * explicit environment, then entry(user), or execve(argv) when entry is NULL.
 * Reports setup/exec errors synchronously. Child remains waitable via PROCESS.
 * Call before creating threads; borrowed callback state is copied by fork.
 * Nonstandard inherited fds remain caller-managed (use close-on-exec). */
int mulib_daemon_launch (const mulib_daemon_t *, mulib_daemon_entry_fn, void *user,
                        mulib_process_t *child);
/** Send SIGTERM to an owned child after checking/reaping an already exited one.
 * A live child still requires process_wait; this function never waits for shutdown. */
int mulib_daemon_stop (mulib_process_t *child);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_DAEMON) && !defined(MULIB_DAEMON_IMPLEMENTED)
#define MULIB_DAEMON_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct mulib_daemon {
  char *name, *description, *directory, *restart;
  char **argv, **env;
  size_t argc, envc;
};
static int
mulib_daemon_error_ (mulib_daemon_error_t *e, int code, size_t at, const char *message)
{
  if (e) {
    e->code = code; e->offset = at;
    (void)snprintf (e->message, sizeof (e->message), "%s", message ? message : "");
  }
  return code;
}
static int
mulib_daemon_text_ (const char *s)
{
  size_t i;
  if (!s) return 0;
  for (i = 0; s[i]; ++i)
    if (i >= 4095 || (unsigned char)s[i] < 32 || (unsigned char)s[i] == 127) return 0;
  return 1;
}
static int
mulib_daemon_identifier_ (const char *s, int name)
{
  size_t i;
  if (!s || !*s) return 0;
  for (i = 0; s[i]; ++i) {
    unsigned char c = (unsigned char)s[i];
    if (i >= 127 || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || c == '_' || ((i || name) && c >= '0' && c <= '9')
        || (name && (c == '-' || c == '.')))) return 0;
  }
  return !name || (strcmp (s, ".") && strcmp (s, ".."));
}
static int
mulib_daemon_set_ (char **dst, const char *s)
{
  char *p;
  size_t n;
  if (!mulib_daemon_text_ (s)) return MULIB_DAEMON_INVALID;
  n = strlen (s) + 1; p = (char *)malloc (n);
  if (!p) return MULIB_DAEMON_MEMORY;
  memcpy (p, s, n); free (*dst); *dst = p;
  return 0;
}
static void
mulib_daemon_vector_free_ (char **v, size_t n)
{
  size_t i;
  for (i = 0; i < n; ++i) free (v[i]);
  free (v);
}
mulib_daemon_t *
mulib_daemon_create (const char *name)
{
  mulib_daemon_t *d;
  if (!mulib_daemon_identifier_ (name, 1)) return NULL;
  d = (mulib_daemon_t *)calloc (1, sizeof (*d));
  if (!d) return NULL;
  if (mulib_daemon_set_ (&d->name, name) || mulib_daemon_set_ (&d->description, name)
      || mulib_daemon_set_ (&d->directory, "/") || mulib_daemon_set_ (&d->restart, "on-failure")) {
    mulib_daemon_destroy (d); return NULL;
  }
  return d;
}
void
mulib_daemon_destroy (mulib_daemon_t *d)
{
  if (!d) return;
  free (d->name); free (d->description); free (d->directory); free (d->restart);
  mulib_daemon_vector_free_ (d->argv, d->argc);
  mulib_daemon_vector_free_ (d->env, d->envc);
  free (d);
}
int mulib_daemon_description (mulib_daemon_t *d, const char *s)
{ return d ? mulib_daemon_set_ (&d->description, s) : MULIB_DAEMON_INVALID; }
int mulib_daemon_directory (mulib_daemon_t *d, const char *s)
{ return d && s && *s == '/' ? mulib_daemon_set_ (&d->directory, s) : MULIB_DAEMON_INVALID; }
int mulib_daemon_restart (mulib_daemon_t *d, const char *s)
{
  return d && s && (!strcmp (s, "no") || !strcmp (s, "on-failure") || !strcmp (s, "always"))
    ? mulib_daemon_set_ (&d->restart, s) : MULIB_DAEMON_INVALID;
}
const char *mulib_daemon_name (const mulib_daemon_t *d) { return d ? d->name : NULL; }
int
mulib_daemon_command (mulib_daemon_t *d, const char *const *argv, size_t n)
{
  char **v;
  size_t i;
  int rc = 0;
  if (!d || !argv || !n || n > 256 || !argv[0] || argv[0][0] != '/') return MULIB_DAEMON_INVALID;
  for (i = 0; i < n; ++i) if (!mulib_daemon_text_ (argv[i])) return MULIB_DAEMON_INVALID;
  v = (char **)calloc (n + 1, sizeof (*v));
  if (!v) return MULIB_DAEMON_MEMORY;
  for (i = 0; i < n && !rc; ++i) rc = mulib_daemon_set_ (v + i, argv[i]);
  if (rc) { mulib_daemon_vector_free_ (v, n); return rc; }
  mulib_daemon_vector_free_ (d->argv, d->argc); d->argv = v; d->argc = n;
  return 0;
}
int
mulib_daemon_environment (mulib_daemon_t *d, const char *key, const char *value)
{
  size_t i, n, m;
  char *entry, **v;
  if (!d || !mulib_daemon_identifier_ (key, 0) || !mulib_daemon_text_ (value)) return MULIB_DAEMON_INVALID;
  n = strlen (key); m = strlen (value);
  for (i = 0; i < d->envc; ++i)
    if (!strncmp (d->env[i], key, n) && d->env[i][n] == '=') break;
  if (i == d->envc && i == 256) return MULIB_DAEMON_INVALID;
  entry = (char *)malloc (n + m + 2);
  if (!entry) return MULIB_DAEMON_MEMORY;
  memcpy (entry, key, n); entry[n] = '='; memcpy (entry+n+1, value, m+1);
  if (i == d->envc) {
    v = (char **)realloc (d->env, (i+2) * sizeof (*v));
    if (!v) { free (entry); return MULIB_DAEMON_MEMORY; }
    d->env = v; d->env[i] = NULL; d->env[++d->envc] = NULL;
  }
  free (d->env[i]); d->env[i] = entry;
  return 0;
}
typedef struct mulib_daemon_binding_ { mulib_daemon_t *daemon; int operation; } mulib_daemon_binding_;
static mulib_mune_status_t
mulib_daemon_native_ (mulib_mune_state_t *state, const mulib_mune_value_t *args,
    size_t count, mulib_mune_result_t *out, void *user)
{
  mulib_daemon_binding_ *b = (mulib_daemon_binding_ *)user;
  const char *strings[256];
  size_t i, n;
  int rc = MULIB_DAEMON_INVALID;
  (void)state; (void)out;
  if (!count || count > 256) return MULIB_MUNE_RUNTIME;
  for (i = 0; i < count; ++i) {
    strings[i] = mulib_mune_value_string (args[i], &n);
    if (!strings[i] || memchr (strings[i], 0, n)) return MULIB_MUNE_RUNTIME;
  }
  if (b->operation == 4) rc = mulib_daemon_command (b->daemon, strings, count);
  else if (b->operation == 5 && count == 2) rc = mulib_daemon_environment (b->daemon, strings[0], strings[1]);
  else if (count == 1) {
    switch (b->operation) {
    case 0: if (mulib_daemon_identifier_ (strings[0], 1)) rc = mulib_daemon_set_ (&b->daemon->name, strings[0]); break;
    case 1: rc = mulib_daemon_description (b->daemon, strings[0]); break;
    case 2: rc = mulib_daemon_directory (b->daemon, strings[0]); break;
    case 3: rc = mulib_daemon_restart (b->daemon, strings[0]); break;
    }
  }
  return rc == MULIB_DAEMON_MEMORY ? MULIB_MUNE_MEMORY : rc ? MULIB_MUNE_RUNTIME : MULIB_MUNE_OK;
}
mulib_daemon_t *
mulib_daemon_load (const char *src, size_t n, mulib_daemon_error_t *error)
{
  static const char *const names[] = { "daemon_name", "daemon_description", "daemon_directory",
    "daemon_restart", "daemon_command", "daemon_environment" };
  mulib_daemon_binding_ bindings[6];
  mulib_daemon_t *d;
  mulib_mune_state_t *state;
  mulib_mune_result_t *result = NULL;
  mulib_mune_error_t e = { MULIB_MUNE_OK, 0, "" };
  size_t i;
  int rc = 0;
  if ((!src && n) || n > 1048576) { mulib_daemon_error_ (error, MULIB_DAEMON_INVALID, 0, "invalid source"); return NULL; }
  d = mulib_daemon_create ("daemon"); state = mulib_mune_state_create (NULL);
  if (!d || !state) rc = mulib_daemon_error_ (error, MULIB_DAEMON_MEMORY, 0, "allocation failed");
  for (i = 0; i < 6 && !rc; ++i) {
    bindings[i].daemon = d; bindings[i].operation = (int)i;
    if (mulib_mune_bind (state, names[i], mulib_daemon_native_, bindings+i))
      rc = mulib_daemon_error_ (error, MULIB_DAEMON_MEMORY, 0, "binding failed");
  }
  if (!rc && mulib_mune_run_source (state, src, n, &result, &e))
    rc = mulib_daemon_error_ (error, MULIB_DAEMON_SCRIPT, e.offset, e.message);
  if (!rc && !d->argc) rc = mulib_daemon_error_ (error, MULIB_DAEMON_INVALID, 0, "daemon_command is required");
  mulib_mune_result_destroy (result); mulib_mune_state_destroy (state);
  if (rc) { mulib_daemon_destroy (d); return NULL; }
  mulib_daemon_error_ (error, 0, 0, "");
  return d;
}
static char *
mulib_daemon_read_ (const char *path, size_t *length, mulib_daemon_error_t *error)
{
  FILE *f;
  char *bytes;
  size_t n;
  if (!path || !*path) { mulib_daemon_error_ (error, MULIB_DAEMON_INVALID, 0, "invalid filename"); return NULL; }
  f = fopen (path, "rb");
  if (!f) { mulib_daemon_error_ (error, MULIB_DAEMON_SYSTEM, 0, "cannot open file"); return NULL; }
  bytes = (char *)malloc (1048577);
  if (!bytes) { fclose (f); mulib_daemon_error_ (error, MULIB_DAEMON_MEMORY, 0, "allocation failed"); return NULL; }
  n = fread (bytes, 1, 1048577, f);
  if (ferror (f) || n > 1048576) {
    fclose (f); free (bytes); mulib_daemon_error_ (error, MULIB_DAEMON_SYSTEM, 0, "read failed or file exceeds 1 MiB"); return NULL;
  }
  if (fclose (f)) { free (bytes); mulib_daemon_error_ (error, MULIB_DAEMON_SYSTEM, 0, "close failed"); return NULL; }
  *length = n;
  return bytes;
}
mulib_daemon_t *
mulib_daemon_load_file (const char *path, mulib_daemon_error_t *error)
{
  size_t n;
  char *src = mulib_daemon_read_ (path, &n, error);
  mulib_daemon_t *d;
  if (!src) return NULL;
  d = mulib_daemon_load (src, n, error); free (src); return d;
}
static int
mulib_daemon_field_ (mulib_mune_state_t *s, mulib_mune_value_t table, const char *key, mulib_mune_value_t value)
{
  mulib_mune_value_t k;
  return !mulib_mune_make_string (s, key, strlen (key), &k) && !mulib_mune_raw_set (s, table, k, value);
}
static int
mulib_daemon_string_field_ (mulib_mune_state_t *s, mulib_mune_value_t table, const char *key, const char *value)
{
  mulib_mune_value_t v;
  return !mulib_mune_make_string (s, value, strlen (value), &v) && mulib_daemon_field_ (s, table, key, v);
}
static int
mulib_daemon_array_ (mulib_mune_state_t *s, mulib_mune_value_t table, const char *key, char *const *values, size_t n)
{
  mulib_mune_value_t array, k, v;
  size_t i;
  if (mulib_mune_make_table (s, &array) || !mulib_daemon_field_ (s, table, key, array)) return 0;
  memset (&k, 0, sizeof (k)); k.type = MULIB_MUNE_INTEGER;
  for (i = 0; i < n; ++i) {
    k.as.integer = (int64_t)i + 1;
    if (mulib_mune_make_string (s, values[i], strlen (values[i]), &v) || mulib_mune_raw_set (s, array, k, v)) return 0;
  }
  return 1;
}
int
mulib_daemon_compile (const mulib_daemon_t *d, const char *src, size_t n,
                      mulib_strbuf_t *out, mulib_daemon_error_t *error)
{
  mulib_mune_state_t *s;
  mulib_mune_value_t table;
  mulib_mune_result_t *result = NULL;
  mulib_mune_error_t e = { MULIB_MUNE_OK, 0, "" };
  const char *bytes;
  size_t length;
  int rc = 0;
  if (!d || !d->argc || !out || (!src && n) || n > 1048576)
    return mulib_daemon_error_ (error, MULIB_DAEMON_INVALID, 0, "invalid specification or profile");
  s = mulib_mune_state_create (NULL);
  if (!s) return mulib_daemon_error_ (error, MULIB_DAEMON_MEMORY, 0, "allocation failed");
  if (mulib_mune_make_table (s, &table) || mulib_mune_set_global (s, "daemon", table)
      || !mulib_daemon_string_field_ (s, table, "name", d->name)
      || !mulib_daemon_string_field_ (s, table, "description", d->description)
      || !mulib_daemon_string_field_ (s, table, "directory", d->directory)
      || !mulib_daemon_string_field_ (s, table, "restart", d->restart)
      || !mulib_daemon_array_ (s, table, "argv", d->argv, d->argc)
      || !mulib_daemon_array_ (s, table, "environment", d->env, d->envc))
    rc = mulib_daemon_error_ (error, MULIB_DAEMON_MEMORY, 0, "profile table allocation failed");
  if (!rc && mulib_mune_run_source (s, src, n, &result, &e))
    rc = mulib_daemon_error_ (error, MULIB_DAEMON_SCRIPT, e.offset, e.message);
  if (!rc) {
    bytes = mulib_mune_value_string (mulib_mune_result_get (result, 0), &length);
    if (mulib_mune_result_count (result) != 1 || !bytes || memchr (bytes, 0, length))
      rc = mulib_daemon_error_ (error, MULIB_DAEMON_SCRIPT, 0, "profile must return one text string");
    else if (!mulib_strbuf_append_n (out, bytes, length))
      rc = mulib_daemon_error_ (error, MULIB_DAEMON_MEMORY, 0, "output allocation failed");
  }
  mulib_mune_result_destroy (result); mulib_mune_state_destroy (s);
  if (!rc) mulib_daemon_error_ (error, 0, 0, "");
  return rc;
}
int
mulib_daemon_compile_file (const mulib_daemon_t *d, const char *path,
                           mulib_strbuf_t *out, mulib_daemon_error_t *error)
{
  size_t n;
  char *src = mulib_daemon_read_ (path, &n, error);
  int rc;
  if (!src) return error ? error->code : MULIB_DAEMON_SYSTEM;
  rc = mulib_daemon_compile (d, src, n, out, error); free (src); return rc;
}
#if !defined(MULIB_DAEMON_NO_POSIX) && !defined(MULIB_PROCESS_NO_POSIX) \
    && (defined(__unix__) || defined(__APPLE__))
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
extern int kill (pid_t, int);
extern char **environ;
static void
mulib_daemon_child_error_ (int fd, int code)
{
  const unsigned char *p = (const unsigned char *)&code;
  size_t left = sizeof (code);
  while (left) {
    ssize_t n = write (fd, p, left);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    p += n; left -= (size_t)n;
  }
}
int
mulib_daemon_launch (const mulib_daemon_t *d, mulib_daemon_entry_fn entry,
                     void *user, mulib_process_t *child)
{
  int pipefd[2], failure = 0;
  pid_t pid;
  size_t used = 0;
  mulib_process_status_t status;
  if (!d || (!entry && !d->argc) || !child || child->pid) return MULIB_DAEMON_INVALID;
  if (mulib_process_pipe (pipefd)) return MULIB_DAEMON_SYSTEM;
  pid = fork ();
  if (pid < 0) {
    int saved = errno; close (pipefd[0]); close (pipefd[1]); errno = saved; return MULIB_DAEMON_SYSTEM;
  }
  if (!pid) {
    int fd, i;
    char *empty[] = { NULL };
    close (pipefd[0]);
    if (setsid () < 0 || chdir (d->directory)) goto child_fail;
    (void)umask (027);
    fd = open ("/dev/null", O_RDWR);
    if (fd < 0) goto child_fail;
    for (i = 0; i < 3; ++i) if (dup2 (fd, i) < 0) goto child_fail;
    if (fd > 2) close (fd);
    if (entry) {
      environ = d->env ? d->env : empty;
      mulib_daemon_child_error_ (pipefd[1], 0); close (pipefd[1]);
      i = entry (user); _exit (i >= 0 && i <= 255 ? i : 1);
    }
    execve (d->argv[0], d->argv, d->env ? d->env : empty);
child_fail:
    mulib_daemon_child_error_ (pipefd[1], errno ? errno : EIO); _exit (127);
  }
  close (pipefd[1]); child->pid = (long)pid;
  while (used < sizeof (failure)) {
    ssize_t n = read (pipefd[0], (unsigned char *)&failure + used, sizeof (failure) - used);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) { failure = errno; break; }
    if (!n) { if (used) failure = EIO; break; }
    used += (size_t)n;
  }
  close (pipefd[0]);
  if (failure) {
    (void)mulib_process_terminate (child, &status); errno = failure; return MULIB_DAEMON_SYSTEM;
  }
  return 0;
}
int
mulib_daemon_stop (mulib_process_t *child)
{
  mulib_process_status_t status;
  int rc = mulib_process_wait (child, &status, 1);
  if (!rc) return 0;
  if (rc != MULIB_PROCESS_RUNNING) return rc == MULIB_PROCESS_INVALID ? MULIB_DAEMON_INVALID : MULIB_DAEMON_SYSTEM;
  return kill ((pid_t)child->pid, SIGTERM) && errno != ESRCH ? MULIB_DAEMON_SYSTEM : 0;
}
#else
int mulib_daemon_launch (const mulib_daemon_t *d, mulib_daemon_entry_fn fn, void *u, mulib_process_t *p)
{ (void)d; (void)fn; (void)u; (void)p; return MULIB_DAEMON_UNSUPPORTED; }
int mulib_daemon_stop (mulib_process_t *p) { (void)p; return MULIB_DAEMON_UNSUPPORTED; }
#endif
#endif
