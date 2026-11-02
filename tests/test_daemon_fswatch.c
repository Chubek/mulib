#define MULIB_IMPL_FSWATCH
#define MULIB_IMPL_SELECT
#define MULIB_IMPL_VFS
#define MULIB_IMPL_DAEMON
#define MULIB_IMPL_PROCESS
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-mune.h"
#include "mulib-process.h"
#include "mulib-select.h"
#include "mulib-vfs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Inject failures only into these new modules, keeping Mune's independent
 * allocation domain intact. Sanitizers check rollback/destruction paths. */
static long allocation_budget = -1;
static void *checked_malloc (size_t n) {
  if (!allocation_budget) { errno = ENOMEM; return NULL; }
  if (allocation_budget > 0) --allocation_budget;
  return malloc (n);
}
static void *checked_calloc (size_t n, size_t size) {
  if (!allocation_budget) { errno = ENOMEM; return NULL; }
  if (allocation_budget > 0) --allocation_budget;
  return calloc (n, size);
}
static void *checked_realloc (void *p, size_t n) {
  if (!allocation_budget) { errno = ENOMEM; return NULL; }
  if (allocation_budget > 0) --allocation_budget;
  return realloc (p, n);
}
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#include "mulib-fswatch.h"
#undef malloc
#undef calloc
#undef realloc

static void library_path (char path[4096], const char *name) {
  const char *file = __FILE__, *slash = strrchr (file, '/');
  int n = snprintf (path, 4096, "%.*s../library/ldaemon/%s", slash ? (int)(slash-file+1) : 0, file, name);
  assert (n > 0 && n < 4096);
}
static void configuration (void) {
  const char source[] =
    "daemon_name('worker')\n"
    "daemon_description('100% complete')\n"
    "daemon_command('/usr/bin/true', 'space argument', '', '$HOME', '%n', 'a\\\"b', 'a\\\\b')\n"
    "daemon_environment('LANG','C')\n"
    "daemon_environment('LANG','en_US')\n";
  mulib_daemon_error_t e;
  mulib_daemon_t *d = mulib_daemon_load (source, sizeof (source)-1, &e);
  mulib_strbuf_t output;
  char path[4096], script[8192];
  const char *badargs[] = { "relative" }, *goodargs[] = { "/usr/bin/true", "" };
  size_t saved;
  assert (d && !e.code && !strcmp (mulib_daemon_name (d), "worker"));
  assert (mulib_daemon_command (d, badargs, 1) == MULIB_DAEMON_INVALID);
  assert (mulib_daemon_environment (d, "A=B", "x") == MULIB_DAEMON_INVALID);
  assert (mulib_daemon_description (d, "line\n[Service]") == MULIB_DAEMON_INVALID);
  assert (mulib_daemon_directory (d, "relative") == MULIB_DAEMON_INVALID);
  assert (mulib_daemon_restart (d, "sometimes") == MULIB_DAEMON_INVALID);
  mulib_strbuf_init (&output);
  library_path (path, "systemd.lua");
  if (mulib_daemon_compile_file (d, path, &output, &e)) fprintf (stderr, "profile: %s at %zu\n", e.message, e.offset);
  assert (!e.code && output.data);
  assert (strstr (output.data, "Description=100%% complete \n"));
  assert (strstr (output.data, "WorkingDirectory=/ \n"));
  assert (strstr (output.data, "\"space argument\" \"\" \"$$HOME\" \"%%n\" \"a\\\"b\" \"a\\\\b\""));
  assert (strstr (output.data, "Environment=\"LANG=en_US\"\n"));
  assert (!strstr (output.data, "LANG=C"));
  saved = output.length;
  assert (mulib_daemon_compile (d, "return 42", 9, &output, &e) == MULIB_DAEMON_SCRIPT);
  assert (output.length == saved && e.message[0]);
  assert (mulib_daemon_compile (d, "return 'x', 'y'", strlen ("return 'x', 'y'"), &output, &e) == MULIB_DAEMON_SCRIPT);
  assert (output.length == saved);
  assert (mulib_daemon_compile (d, "return '\\0'", strlen ("return '\\0'"), &output, &e) == MULIB_DAEMON_SCRIPT);
  assert (output.length == saved);
  assert (!mulib_daemon_command (d, goodargs, 2));
  mulib_daemon_destroy (d); mulib_strbuf_free (&output);
  assert (!mulib_daemon_load (NULL, 0, &e) && e.code == MULIB_DAEMON_INVALID);
  assert (!mulib_daemon_load ("daemon_command(", 15, &e) && e.code == MULIB_DAEMON_SCRIPT);
  assert (!mulib_daemon_create ("bad/name"));
  library_path (path, "service.lua");
  assert (snprintf (script, sizeof (script),
    "local service=dofile('%s'); service{name='configured',argv={'/usr/bin/true'},environment={Z='last',A='first'}}", path) > 0);
  d = mulib_daemon_load (script, strlen (script), &e);
  if (!d) fprintf (stderr, "service: %s at %zu\n", e.message, e.offset);
  assert (d && !strcmp (mulib_daemon_name (d), "configured"));
  mulib_strbuf_init (&output); library_path (path, "systemd.lua");
  assert (!mulib_daemon_compile_file (d, path, &output, &e));
  assert (strstr (output.data, "A=first") < strstr (output.data, "Z=last"));
  mulib_strbuf_free (&output); mulib_daemon_destroy (d);
  library_path (path, "fswatch.lua");
  assert (snprintf (script, sizeof (script), "dofile('%s')('watcher','/usr/bin/true','/config/watch.fsw')", path) > 0);
  d = mulib_daemon_load (script, strlen (script), &e); assert (d);
  mulib_daemon_destroy (d);
}
typedef struct log_state { unsigned masks[16]; char paths[16][64]; size_t count; int fail; } log_state;
static int log_change (void *user, const char *handler, const mulib_fswatch_change_t *e) {
  log_state *log = (log_state *)user;
  assert (!strcmp (handler, "record") && log->count < 16);
  assert (strlen (e->path) < sizeof (log->paths[0]));
  strcpy (log->paths[log->count], e->path); log->masks[log->count++] = e->mask;
  return log->fail;
}
static void grammar (void) {
  static const char *const bad[] = {
    "watch", "watch \"\" {}", "watch \"/\" {", "watch \"/\" { on wat \"*\" -> record; }",
    "watch \"/\" { on create| \"*\" -> record; }", "watch \"/\" { on create \"*\" -> record }",
    "watch \"/\" recursive recursive {}", "watch \"/\" { on create \"*\" -> 123; }",
    "watch \"/\" {} garbage", "watch \"bad\\q\" {}", "watch \"unfinished"
  };
  size_t i;
  mulib_fswatch_error_t e;
  mulib_fswatch_program_t *p = mulib_fswatch_parse (NULL, 0, &e);
  assert (p && !e.message && !mulib_fswatch_program_count (p)); mulib_fswatch_program_destroy (p);
  for (i = 0; i < sizeof (bad)/sizeof (bad[0]); ++i) {
    assert (!mulib_fswatch_parse (bad[i], strlen (bad[i]), &e));
    assert (e.message && e.offset <= strlen (bad[i]));
  }
  assert (!mulib_fswatch_parse (NULL, 1, &e));
  assert (!mulib_fswatch_parse ("", 65537, &e));
  assert (!mulib_fswatch_parse ("\0", 1, &e));
}
static void virtual_filesystem (void) {
  const char source[] = "# source files\nwatch \"/src\" recursive { on any \"*.c\" -> record; }";
  mulib_fswatch_program_t *p = mulib_fswatch_parse (source, sizeof (source)-1, NULL);
  mulib_fswatch_runtime_t *r, *small;
  mulib_vfs vfs;
  mulib_vfs_mem mem;
  mulib_vfs_ops ops = mulib_vfs_mem_ops ();
  log_state log = { {0}, {{0}}, 0, 0 };
  int recursive;
  assert (p && mulib_fswatch_program_count (p) == 1);
  assert (!strcmp (mulib_fswatch_program_root (p, 0, &recursive), "/src") && recursive);
  assert (!mulib_vfs_init (&vfs, 1) && !mulib_vfs_mem_init (&mem, 16, 1024));
  assert (!mulib_vfs_mount (&vfs, "/src", &ops, &mem));
  assert (!mulib_vfs_mem_mkdir (&mem, "/nested"));
  r = mulib_fswatch_runtime_create (p, mulib_fswatch_vfs_scan, &vfs, 32);
  small = mulib_fswatch_runtime_create (p, mulib_fswatch_vfs_scan, &vfs, 1);
  assert (r && small && !mulib_fswatch_poll (r, log_change, &log) && !log.count);
  assert (mulib_fswatch_poll (small, log_change, &log) == -1 && errno == EOVERFLOW);
  mulib_fswatch_runtime_destroy (small);
  assert (!mulib_vfs_mem_put (&mem, "/nested/a.c", "abc", 3));
  assert (!mulib_vfs_mem_put (&mem, "/skip.txt", "abc", 3));
  assert (!mulib_fswatch_poll (r, log_change, &log) && log.count == 1);
  assert (!strcmp (log.paths[0], "nested/a.c") && log.masks[0] == MULIB_FSWATCH_CREATE);
  assert (!mulib_vfs_mem_put (&mem, "/nested/a.c", "xyz", 3));
  assert (!mulib_fswatch_poll (r, log_change, &log) && log.count == 2 && log.masks[1] == MULIB_FSWATCH_MODIFY);
  assert (!mulib_fswatch_poll (r, log_change, &log) && log.count == 2);
  assert (!mulib_vfs_mem_remove (&mem, "/nested/a.c"));
  log.fail = 17;
  assert (mulib_fswatch_poll (r, log_change, &log) == 17 && log.count == 3 && log.masks[2] == MULIB_FSWATCH_DELETE);
  assert (!mulib_fswatch_poll (r, log_change, &log) && log.count == 3);
  mulib_fswatch_runtime_destroy (r); mulib_fswatch_program_destroy (p);
  mulib_vfs_free (&vfs); mulib_vfs_mem_free (&mem);
}
typedef struct scanner { int fail, duplicate; uint64_t version; } scanner;
static int synthetic_scan (void *fs, const char *root, int recursive, mulib_fswatch_snapshot_fn emit, void *user) {
  scanner *s = (scanner *)fs;
  int rc;
  (void)root; (void)recursive;
  rc = emit (user, "a.c", ++s->version, 0);
  if (rc) return rc;
  if (s->duplicate) return emit (user, "a.c", s->version, 0);
  if (s->fail) { errno = EIO; return -1; }
  return 0;
}
static void scan_failure (void) {
  const char source[] = "watch \"object://repo\" { on any \"*\" -> record; }";
  mulib_fswatch_program_t *p = mulib_fswatch_parse (source, sizeof (source)-1, NULL);
  scanner s = {0, 0, 0};
  mulib_fswatch_runtime_t *r = mulib_fswatch_runtime_create (p, synthetic_scan, &s, 8);
  log_state log = { {0}, {{0}}, 0, 0 };
  assert (r && !mulib_fswatch_poll (r, log_change, &log));
  s.fail = 1;
  assert (mulib_fswatch_poll (r, log_change, &log) == -1 && !log.count);
  s.fail = 0; s.duplicate = 1;
  assert (mulib_fswatch_poll (r, log_change, &log) == -1 && errno == EINVAL && !log.count);
  s.duplicate = 0;
  allocation_budget = 0;
  assert (mulib_fswatch_poll (r, log_change, &log) == -1 && errno == ENOMEM && !log.count);
  allocation_budget = 1;
  assert (mulib_fswatch_poll (r, log_change, &log) == -1 && errno == ENOMEM && !log.count);
  allocation_budget = -1;
  assert (!mulib_fswatch_poll (r, log_change, &log) && log.count == 1 && log.masks[0] == MULIB_FSWATCH_MODIFY);
  mulib_fswatch_runtime_destroy (r); mulib_fswatch_program_destroy (p);
}
static void allocation_failures (void) {
  const char source[] = "watch \"/\" { on any \"*\" -> record; }";
  const char *command[] = { "/bin/true", "value" };
  mulib_daemon_t *d;
  mulib_strbuf_t output;
  const char profile[] = "return daemon.argv[2] .. daemon.environment[1]";
  long i;
  for (i = 0; i < 4; ++i) {
    allocation_budget = i;
    assert (!mulib_fswatch_parse (source, sizeof (source)-1, NULL));
  }
  for (i = 0; i < 5; ++i) {
    allocation_budget = i; assert (!mulib_daemon_create ("test"));
  }
  allocation_budget = -1;
  d = mulib_daemon_create ("test");
  assert (d && !mulib_daemon_command (d, command, 2));
  assert (!mulib_daemon_environment (d, "KEY", "old"));
  command[1] = "replacement";
  for (i = 0; i < 3; ++i) {
    allocation_budget = i;
    assert (mulib_daemon_command (d, command, 2) == MULIB_DAEMON_MEMORY);
  }
  allocation_budget = 0;
  assert (mulib_daemon_environment (d, "KEY", "new") == MULIB_DAEMON_MEMORY);
  allocation_budget = 1;
  assert (mulib_daemon_environment (d, "SECOND", "new") == MULIB_DAEMON_MEMORY);
  allocation_budget = -1;
  mulib_strbuf_init (&output);
  assert (!mulib_daemon_compile (d, profile, sizeof (profile)-1, &output, NULL));
  assert (!strcmp (output.data, "valueKEY=old"));
  mulib_strbuf_free (&output); mulib_daemon_destroy (d);
}
#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
static int child_entry (void *user) {
  const char *v = getenv ("MULIB_DAEMON_TEST");
  char cwd[32];
  (void)user;
  return v && !strcmp (v, "explicit") && getcwd (cwd, sizeof (cwd)) && !strcmp (cwd, "/") ? 23 : 99;
}
static int pipe_change (void *user, const char *handler, const mulib_fswatch_change_t *e) {
  int fd = *(int *)user;
  (void)handler; (void)e;
  return write (fd, "x", 1) == 1 ? 0 : -1;
}
static void processes (void) {
  mulib_daemon_t *d = mulib_daemon_create ("test");
  mulib_process_t child;
  mulib_process_status_t status;
  const char *missing[] = { "/mulib-no-such-executable" };
  const char *args[] = { "/bin/sh", "-c", "test \"$MULIB_DAEMON_TEST\" = explicit && exit 19; exit 98" };
  const char source[] = "watch \"virtual\" { on modify \"*\" -> record; }";
  mulib_fswatch_program_t *p;
  scanner s = {0, 0, 0};
  mulib_select_entry wait;
  int fds[2];
  char byte;
  assert (d && !mulib_daemon_environment (d, "MULIB_DAEMON_TEST", "explicit"));
  mulib_process_init (&child);
  assert (!mulib_daemon_launch (d, child_entry, NULL, &child));
  assert (!mulib_process_wait (&child, &status, 0) && status.exited && status.exit_code == 23);
  assert (!mulib_daemon_command (d, missing, 1));
  assert (mulib_daemon_launch (d, NULL, NULL, &child) == MULIB_DAEMON_SYSTEM && !child.pid && errno == ENOENT);
  assert (!mulib_daemon_command (d, args, 3));
  assert (!mulib_daemon_launch (d, NULL, NULL, &child));
  assert (!mulib_process_wait (&child, &status, 0) && status.exited && status.exit_code == 19);
  assert (!mulib_daemon_directory (d, "/mulib-no-such-directory"));
  assert (mulib_daemon_launch (d, child_entry, NULL, &child) == MULIB_DAEMON_SYSTEM && !child.pid);
  assert (!mulib_daemon_directory (d, "/"));
  p = mulib_fswatch_parse (source, sizeof (source)-1, NULL); assert (p);
  assert (!mulib_process_pipe (fds));
  assert (!mulib_fswatch_launch (d, p, synthetic_scan, &s, 8, pipe_change, &fds[1], 10, &child));
  close (fds[1]); wait.fd = fds[0]; wait.events = MULIB_SELECT_READ; wait.revents = 0;
  assert (mulib_select_wait (&wait, 1, 3000) == 1);
  assert (read (fds[0], &byte, 1) == 1 && byte == 'x');
  assert (!mulib_daemon_stop (&child));
  assert (!mulib_process_wait (&child, &status, 0) && status.term_signal == SIGTERM);
  close (fds[0]); mulib_fswatch_program_destroy (p); mulib_daemon_destroy (d);
}
static void native_filesystem (void) {
  char root[128], child[160], sub[160], script[512];
  FILE *f;
  mulib_fswatch_program_t *p;
  mulib_fswatch_runtime_t *r;
  log_state log = { {0}, {{0}}, 0, 0 };
  assert (snprintf (root, sizeof (root), "mulib-fswatch-program-%ld", (long)getpid ()) > 0);
  assert (!mkdir (root, 0700));
  assert (snprintf (sub, sizeof (sub), "%s/sub", root) > 0 && !mkdir (sub, 0700));
  assert (snprintf (child, sizeof (child), "%s/sub/a.c", root) > 0);
  assert (snprintf (script, sizeof (script), "watch \"%s\" recursive { on any \"*.c\" -> record; }", root) > 0);
  p = mulib_fswatch_parse (script, strlen (script), NULL); assert (p);
  r = mulib_fswatch_runtime_create (p, mulib_fswatch_native_scan, NULL, 32);
  assert (r && !mulib_fswatch_poll (r, log_change, &log));
  f = fopen (child, "wb"); assert (f && fwrite ("abc", 1, 3, f) == 3 && !fclose (f));
  assert (!mulib_fswatch_poll (r, log_change, &log) && log.count == 1 && log.masks[0] == MULIB_FSWATCH_CREATE);
  f = fopen (child, "wb"); assert (f && fwrite ("xyz", 1, 3, f) == 3 && !fclose (f));
  assert (!mulib_fswatch_poll (r, log_change, &log) && log.count == 2 && log.masks[1] == MULIB_FSWATCH_MODIFY);
  assert (!unlink (child));
  assert (!mulib_fswatch_poll (r, log_change, &log) && log.count == 3 && log.masks[2] == MULIB_FSWATCH_DELETE);
  mulib_fswatch_runtime_destroy (r); mulib_fswatch_program_destroy (p);
  assert (!rmdir (sub) && !rmdir (root));
}
#endif
int main (int argc, char **argv) {
  if (argc == 2 && !strcmp (argv[1], "--systemd")) {
    mulib_daemon_t *d = mulib_daemon_create ("profile-test");
    const char *args[] = { "/bin/true", "", ";", "$HOME", "${HOME}", "%n", "a\"b", "a\\b", "two words" };
    mulib_strbuf_t output;
    char path[4096];
    assert (d && !mulib_daemon_command (d, args, sizeof (args)/sizeof (args[0])));
    assert (!mulib_daemon_description (d, "literal 100% and trailing backslash\\"));
    assert (!mulib_daemon_environment (d, "LITERAL", "$HOME %n \"quoted\" \\"));
    mulib_strbuf_init (&output); library_path (path, "systemd.lua");
    assert (!mulib_daemon_compile_file (d, path, &output, NULL));
    fputs (output.data, stdout);
    mulib_strbuf_free (&output); mulib_daemon_destroy (d);
    return 0;
  }
  configuration (); grammar (); virtual_filesystem (); scan_failure (); allocation_failures ();
#if defined(__unix__) || defined(__APPLE__)
  processes (); native_filesystem ();
#endif
  return 0;
}
