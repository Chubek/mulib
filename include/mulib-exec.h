/** @file mulib-exec.h
 * @brief POSIX command execution, concurrent pipelines and a documented Mune DSL.
 * Define MULIB_IMPL_EXEC and dependency switches (see manual/mulib-exec.md).
 */
#ifndef MULIB_EXEC_H
#define MULIB_EXEC_H
#include "mulib-process.h"
#include "mulib-perf.h"
#include "mulib-mune.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_exec_backend {
  MULIB_EXEC_AUTO, MULIB_EXEC_FORK, MULIB_EXEC_SPAWN
} mulib_exec_backend_t;
enum {
  MULIB_EXEC_OK = 0, MULIB_EXEC_INVALID = -1, MULIB_EXEC_MEMORY = -2,
  MULIB_EXEC_SYSTEM = -3, MULIB_EXEC_UNSUPPORTED = -4,
  MULIB_EXEC_TIMEOUT = -5, MULIB_EXEC_LIMIT = -6
};
/** Borrowed command specification. Initialize with mulib_exec_command_init.
 * argv contains argc strings (including argv[0]); file defaults to argv[0].
 * env==NULL inherits environ; otherwise envc NAME=value strings replace it.
 * PATH is taken from that environment, defaulting to /bin:/usr/bin if absent.
 * cwd is child-only. AUTO selects fork for cwd, spawn otherwise; explicit
 * SPAWN with cwd returns UNSUPPORTED on this portable POSIX implementation.
 * io descriptors are borrowed, -1 inherits, >=3 redirects. Paths override io,
 * are relative to child cwd, and output files use mode 0666 subject to umask.
 * merge_stderr duplicates the final stdout. No shell expansion is performed.
 * doc is an optional borrowed descriptive string, never executed.
 */
typedef struct mulib_exec_command {
  const char *file, *const *argv;
  size_t argc;
  const char *const *env;
  size_t envc;
  const char *cwd, *stdin_path, *stdout_path, *stderr_path, *doc;
  mulib_process_io_t io;
  mulib_exec_backend_t backend;
  int append_stdout, append_stderr, merge_stderr;
} mulib_exec_command_t;
/** Initialize defaults: inherited environment/stdio and automatic backend. */
void mulib_exec_command_init (mulib_exec_command_t *);
/** Start one child; out must be initialized with process_init and unused.
 * Reports setup/exec failures synchronously (errno is preserved). The caller
 * owns the child and must wait/terminate it using mulib-process.h. All other
 * inherited descriptors remain caller-managed; mark them close-on-exec.
 */
int mulib_exec_start (const mulib_exec_command_t *, mulib_process_t *out);
/** Signal an owned, live child; first checks for exit. Returns PROCESS_RUNNING
 * after sending, OK if already reaped, negative on failure. No group signalling.
 * A successfully signalled child still needs process_wait. */
int mulib_exec_signal (mulib_process_t *, int signal_number,
                       mulib_process_status_t *);
/** Zero initialization selects inherited input, no capture, no timeout,
 * last-stage status. input!=NULL supplies input_size binary bytes via a
 * temporary regular file (empty string supplies EOF); it is not a tty/pipe.
 * capture flags collect final stdout and aggregate stderr concurrently.
 * max_output defaults to 16 MiB per stream. timeout_ms covers launch/drain/wait
 * and uses a monotonic clock. A timeout kills/reaps direct children only.
 */
typedef struct mulib_exec_options {
  const void *input;
  size_t input_size, max_output;
  unsigned timeout_ms;
  int capture_stdout, capture_stderr, pipefail;
} mulib_exec_options_t;
/** Owned result, initialized to zero before use. Even on error, holds partial
 * capture and reaped statuses. code is last-stage status or rightmost nonzero
 * with pipefail; signalled stages map to 128+signal. API errors are separate
 * from program exit codes. system_error is an errno value; failed_stage is
 * SIZE_MAX unless a particular launch failed. Captures are NUL-terminated but
 * lengths are authoritative (binary/NUL bytes are retained).
 */
typedef struct mulib_exec_result {
  char *out, *err;
  size_t out_size, err_size, count, failed_stage;
  mulib_process_status_t *stages;
  int code, system_error;
} mulib_exec_result_t;
/** Free a result and reset it to zero; NULL safe. Never use on live children. */
void mulib_exec_result_destroy (mulib_exec_result_t *);
/** Run 1..256 commands connected by pipes; launch all before waiting. On any
 * launch/I/O/allocation/timeout error close pipes and kill/reap started children.
 * Internal stdin/stdout redirections and conflicting capture are rejected.
 * out must be zero-initialized or destroyed. Child handles cannot be reaped by
 * another thread or SIGCHLD handler. No process-global cwd/signal changes.
 */
int mulib_exec_pipeline (const mulib_exec_command_t *, size_t,
                          const mulib_exec_options_t *, mulib_exec_result_t *out);
/** Convenience single-command fork/spawn/communicate/wait operation. */
int mulib_exec_run (const mulib_exec_command_t *, const mulib_exec_options_t *,
                      mulib_exec_result_t *);
/** Execute an executable file directly (kernel shebang handling, no shell
 * fallback). settings may supply argv/argc, cwd, environment and redirects;
 * NULL settings supplies argv[0]=path. All strings are borrowed for the call. */
int mulib_exec_run_file (const char *path, const mulib_exec_command_t *settings,
                         const mulib_exec_options_t *, mulib_exec_result_t *);
/** Explicit shell execution using /bin/sh -c text. settings supplies child
 * environment, cwd and redirects; its file/argv are ignored. This executes
 * trusted code, not a quoting/escaping API or an isolation boundary. */
int mulib_exec_run_text (const char *text, const mulib_exec_command_t *settings,
                         const mulib_exec_options_t *, mulib_exec_result_t *);
/** Install global exec DSL into an existing Mune state. exec.command{argv=...,
 * doc=[[...]]}, exec.pipeline(...), exec.run(plan,options), exec.capture,
 * exec.check and exec.sequence are documented in the manual and exec.docs.
 * Plans are ordinary Lua tables, and execution is explicit. Mune scripts have
 * ordinary host capabilities. Binding borrows no C context. */
mulib_mune_status_t mulib_exec_bind (mulib_mune_state_t *, mulib_mune_error_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_EXEC) && !defined(MULIB_EXEC_IMPLEMENTED)
#define MULIB_EXEC_IMPLEMENTED
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void
mulib_exec_command_init (mulib_exec_command_t *c)
{
  if (c) { memset (c, 0, sizeof (*c)); c->io.stdin_fd = c->io.stdout_fd = c->io.stderr_fd = -1; }
}
void
mulib_exec_result_destroy (mulib_exec_result_t *r)
{
  if (r) { free (r->out); free (r->err); free (r->stages); memset (r, 0, sizeof (*r)); }
}
#if !defined(MULIB_EXEC_NO_POSIX) && !defined(MULIB_PROCESS_NO_POSIX) \
    && (defined(__unix__) || defined(__APPLE__))
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
extern int kill (pid_t, int);
extern int fileno (FILE *);
/* Serialize descriptor construction and launches made through EXEC. Portable
 * pipe/fcntl and tmpfile/fcntl sequences are not atomically close-on-exec.
 * Only the launch phase is locked; children run and drain concurrently. Other
 * process-launch APIs still need application-level coordination on fallback OSes. */
static pthread_mutex_t mulib_exec_launch_mutex_ = PTHREAD_MUTEX_INITIALIZER;
static int
mulib_exec_valid_ (const mulib_exec_command_t *c)
{
  size_t i;
  int fds[3];
  if (!c || !c->argv || !c->argc || c->argc > 65536 || c->envc > 65536
      || (!c->env && c->envc) || c->backend < MULIB_EXEC_AUTO || c->backend > MULIB_EXEC_SPAWN)
    return 0;
  for (i = 0; i < c->argc; ++i) if (!c->argv[i]) return 0;
  if (!*(c->file ? c->file : c->argv[0])) return 0;
  for (i = 0; i < c->envc; ++i)
    if (!c->env[i] || !strchr (c->env[i], '=') || c->env[i][0] == '=') return 0;
  fds[0] = c->io.stdin_fd; fds[1] = c->io.stdout_fd; fds[2] = c->io.stderr_fd;
  for (i = 0; i < 3; ++i)
    if (fds[i] < -1 || (fds[i] >= 0 && fds[i] < 3 && fds[i] != (int)i)
        || (fds[i] >= 0 && fcntl (fds[i], F_GETFD) < 0)) return 0;
  return !(c->merge_stderr && c->stderr_path);
}
static void
mulib_exec_paths_free_ (char **paths)
{
  size_t i;
  if (paths) { for (i = 0; paths[i]; ++i) free (paths[i]); free (paths); }
}
/* Prepare every allocation and PATH candidate before fork. The child uses
 * only async-signal-safe calls, including the error-pipe write and _exit. */
static char **
mulib_exec_paths_ (const char *file, char **env)
{
  const char *path = "/bin:/usr/bin", *p, *q;
  size_t n = 1, i, len = strlen (file);
  char **v;
  for (i = 0; env && env[i]; ++i)
    if (!strncmp (env[i], "PATH=", 5)) { path = env[i] + 5; break; }
  if (strchr (file, '/')) path = "";
  for (p = path; *p; ++p) if (*p == ':') ++n;
  if (n > SIZE_MAX / sizeof (*v) - 1) { errno = ENOMEM; return NULL; }
  v = (char **)calloc (n + 1, sizeof (*v));
  if (!v) return NULL;
  p = path;
  for (i = 0; i < n; ++i) {
    size_t k;
    q = strchr (p, ':'); k = q ? (size_t)(q - p) : strlen (p);
    if (k > SIZE_MAX - len - 2) { errno = ENOMEM; goto fail; }
    v[i] = (char *)malloc (k + len + 2);
    if (!v[i]) goto fail;
    memcpy (v[i], p, k);
    if (k) v[i][k++] = '/';
    memcpy (v[i] + k, file, len + 1);
    if (q) p = q + 1;
  }
  return v;
fail:
  mulib_exec_paths_free_ (v); return NULL;
}
static int
mulib_exec_start_ (const mulib_exec_command_t *c, mulib_process_t *out)
{
  char **argv = NULL, **env = NULL, **paths = NULL;
  const char *redirect[3];
  int fds[3], flags[3], rc = 0, result = MULIB_EXEC_SYSTEM, denied = 0;
  size_t i, j;
  pid_t pid;
  if (!out || out->pid || !mulib_exec_valid_ (c)) return MULIB_EXEC_INVALID;
  if (c->cwd && c->backend == MULIB_EXEC_SPAWN) return MULIB_EXEC_UNSUPPORTED;
  argv = (char **)calloc (c->argc + 1, sizeof (*argv));
  if (!argv) return MULIB_EXEC_MEMORY;
  for (i = 0; i < c->argc; ++i) argv[i] = (char *)c->argv[i];
  if (c->env) {
    env = (char **)calloc (c->envc + 1, sizeof (*env));
    if (!env) { result = MULIB_EXEC_MEMORY; goto done; }
    for (i = 0; i < c->envc; ++i) env[i] = (char *)c->env[i];
  }
  paths = mulib_exec_paths_ (c->file ? c->file : c->argv[0], c->env ? env : environ);
  if (!paths) { result = MULIB_EXEC_MEMORY; goto done; }
  fds[0] = c->io.stdin_fd; fds[1] = c->io.stdout_fd; fds[2] = c->io.stderr_fd;
  redirect[0] = c->stdin_path; redirect[1] = c->stdout_path; redirect[2] = c->stderr_path;
  flags[0] = O_RDONLY;
  flags[1] = O_WRONLY | O_CREAT | (c->append_stdout ? O_APPEND : O_TRUNC);
  flags[2] = O_WRONLY | O_CREAT | (c->append_stderr ? O_APPEND : O_TRUNC);
  if (c->backend != MULIB_EXEC_FORK && !c->cwd) {
    posix_spawn_file_actions_t a;
    rc = posix_spawn_file_actions_init (&a);
    if (rc) goto done;
    for (i = 0; i < 3 && !rc; ++i) {
      if (redirect[i]) rc = posix_spawn_file_actions_addopen (&a, (int)i, redirect[i], flags[i], 0666);
      else if (fds[i] >= 0) rc = posix_spawn_file_actions_adddup2 (&a, fds[i], (int)i);
    }
    if (!rc && c->merge_stderr) rc = posix_spawn_file_actions_adddup2 (&a, 1, 2);
    for (i = 0; i < 3 && !rc; ++i) if (fds[i] >= 3) {
      for (j = 0; j < i && fds[j] != fds[i]; ++j) {}
      if (j == i) rc = posix_spawn_file_actions_addclose (&a, fds[i]);
    }
    if (!rc) {
      for (i = 0; paths[i]; ++i) {
        rc = posix_spawn (&pid, paths[i], &a, NULL, argv, c->env ? env : environ);
        if (!rc) break;
        if (rc == EACCES) denied = 1;
        else if (rc != ENOENT && rc != ENOTDIR) break;
      }
      if (rc && !paths[i] && denied) rc = EACCES;
    }
    posix_spawn_file_actions_destroy (&a);
    if (!rc) { out->pid = (long)pid; result = MULIB_EXEC_OK; }
  } else {
    int ep[2], child_error = 0;
    size_t got = 0;
    ssize_t nr;
    if (mulib_process_pipe (ep)) goto done;
    pid = fork ();
    if (pid < 0) { rc = errno; close (ep[0]); close (ep[1]); goto done; }
    if (!pid) {
      close (ep[0]);
      if (c->cwd && chdir (c->cwd)) goto child_fail;
      for (i = 0; i < 3; ++i) {
        if (redirect[i]) {
          int fd = open (redirect[i], flags[i], 0666);
          if (fd < 0) goto child_fail;
          if (fd != (int)i) {
            if (dup2 (fd, (int)i) < 0) { close (fd); goto child_fail; }
            close (fd);
          }
        } else if (fds[i] >= 0) {
          if (fds[i] != (int)i) { if (dup2 (fds[i], (int)i) < 0) goto child_fail; }
          else if (fcntl ((int)i, F_SETFD, 0) < 0) goto child_fail;
        }
      }
      if (c->merge_stderr && dup2 (1, 2) < 0) goto child_fail;
      for (i = 0; i < 3; ++i) if (fds[i] >= 3) {
        for (j = 0; j < i && fds[j] != fds[i]; ++j) {}
        if (j == i) close (fds[i]);
      }
      for (i = 0; paths[i]; ++i) {
        execve (paths[i], argv, c->env ? env : environ);
        if (errno == EACCES) denied = 1;
        else if (errno != ENOENT && errno != ENOTDIR) break;
      }
      if (!paths[i] && denied) errno = EACCES;
child_fail:
      child_error = errno;
      while (got < sizeof (child_error)) {
        nr = write (ep[1], (char *)&child_error + got, sizeof (child_error) - got);
        if (nr > 0) got += (size_t)nr;
        else if (nr < 0 && errno == EINTR) continue;
        else break;
      }
      _exit (127);
    }
    close (ep[1]);
    while (got < sizeof (child_error)) {
      nr = read (ep[0], (char *)&child_error + got, sizeof (child_error) - got);
      if (nr > 0) got += (size_t)nr;
      else if (nr < 0 && errno == EINTR) continue;
      else { if (nr < 0) rc = errno; break; }
    }
    close (ep[0]);
    out->pid = (long)pid;
    if (got || rc) {
      mulib_process_status_t st;
      if (!rc) rc = got == sizeof (child_error) ? child_error : EIO;
      (void)mulib_process_terminate (out, &st);
    } else result = MULIB_EXEC_OK;
  }
done:
  if (!rc && result) rc = errno;
  mulib_exec_paths_free_ (paths); free (argv); free (env);
  if (rc) errno = rc;
  return result;
}
int
mulib_exec_start (const mulib_exec_command_t *c, mulib_process_t *out)
{
  int rc, saved;
  rc = pthread_mutex_lock (&mulib_exec_launch_mutex_);
  if (rc) { errno = rc; return MULIB_EXEC_SYSTEM; }
  rc = mulib_exec_start_ (c, out); saved = errno;
  (void)pthread_mutex_unlock (&mulib_exec_launch_mutex_); errno = saved;
  return rc;
}
int
mulib_exec_signal (mulib_process_t *p, int sig, mulib_process_status_t *st)
{
  int rc;
  if (sig < 0) return MULIB_EXEC_INVALID;
  rc = mulib_process_wait (p, st, 1);
  if (rc != MULIB_PROCESS_RUNNING) return rc;
  if (kill ((pid_t)p->pid, sig)) return MULIB_EXEC_SYSTEM;
  return MULIB_PROCESS_RUNNING;
}
static void
mulib_exec_close_ (int *fd)
{
  if (*fd >= 0) { close (*fd); *fd = -1; }
}
static int
mulib_exec_capture_ (int *fd, char **buf, size_t *length, size_t max)
{
  char bytes[8192], *p;
  ssize_t n = read (*fd, bytes, sizeof (bytes));
  if (!n) { mulib_exec_close_ (fd); return 0; }
  if (n < 0) return errno == EINTR || errno == EAGAIN ? 0 : MULIB_EXEC_SYSTEM;
  if ((size_t)n > max - *length) return MULIB_EXEC_LIMIT;
  p = (char *)realloc (*buf, *length + (size_t)n + 1);
  if (!p) return MULIB_EXEC_MEMORY;
  memcpy (p + *length, bytes, (size_t)n); *length += (size_t)n;
  p[*length] = 0; *buf = p; return 0;
}
int
mulib_exec_pipeline (const mulib_exec_command_t *commands, size_t count,
                     const mulib_exec_options_t *options, mulib_exec_result_t *r)
{
  mulib_exec_options_t defaults = {0};
  const mulib_exec_options_t *o = options ? options : &defaults;
  mulib_process_t *children = NULL;
  int output[2] = {-1,-1}, errors[2] = {-1,-1}, link[2] = {-1,-1}, previous = -1;
  int rc = 0, saved = 0, input_fd = -1, launch_locked = 0;
  FILE *input = NULL;
  size_t i, active, max = o->max_output ? o->max_output : 16 * 1024 * 1024;
  uint64_t begin = 0, now;
  if (!commands || !count || count > 256 || !r || r->out || r->err || r->stages
      || (!o->input && o->input_size) || max == SIZE_MAX) return MULIB_EXEC_INVALID;
  for (i = 0; i < count; ++i) {
    const mulib_exec_command_t *c = commands + i;
    if (!mulib_exec_valid_ (c)
        || ((i || o->input) && (c->stdin_path || c->io.stdin_fd != -1))
        || ((i + 1 < count || o->capture_stdout) && (c->stdout_path || c->io.stdout_fd != -1))
        || (o->capture_stderr && (c->stderr_path || c->io.stderr_fd != -1))) return MULIB_EXEC_INVALID;
  }
  memset (r, 0, sizeof (*r)); r->failed_stage = SIZE_MAX;
  children = (mulib_process_t *)calloc (count, sizeof (*children));
  r->stages = (mulib_process_status_t *)calloc (count, sizeof (*r->stages));
  if (!children || !r->stages) { rc = MULIB_EXEC_MEMORY; goto done; }
  r->count = count;
  if (o->timeout_ms && mulib_perf_now (&begin)) { rc = MULIB_EXEC_SYSTEM; goto done; }
  saved = pthread_mutex_lock (&mulib_exec_launch_mutex_);
  if (saved) { errno = saved; rc = MULIB_EXEC_SYSTEM; goto done; }
  launch_locked = 1;
  if (o->input) {
    input = tmpfile ();
    if (!input) { rc = MULIB_EXEC_SYSTEM; goto done; }
    input_fd = fileno (input);
    if (input_fd < 0 || fcntl (input_fd, F_SETFD, FD_CLOEXEC) < 0
        || fwrite (o->input, 1, o->input_size, input) != o->input_size
        || fflush (input) || fseek (input, 0, SEEK_SET)) { rc = MULIB_EXEC_SYSTEM; goto done; }
    /* Duplicate above stdio even when the application closed fd 0/1/2. */
    previous = fcntl (input_fd, F_DUPFD, 3);
    if (previous < 0 || fcntl (previous, F_SETFD, FD_CLOEXEC) < 0) { rc = MULIB_EXEC_SYSTEM; goto done; }
    /* tmpfile may have occupied a previously closed standard descriptor.
     * Retain only the duplicate >=3 before starting any child. */
    if (fclose (input)) { input = NULL; rc = MULIB_EXEC_SYSTEM; goto done; }
    input = NULL;
  }
  if (o->capture_stdout) {
    r->out = (char *)calloc (1, 1);
    if (!r->out) { rc = MULIB_EXEC_MEMORY; goto done; }
    if (mulib_process_pipe (output)) { rc = MULIB_EXEC_SYSTEM; goto done; }
  }
  if (o->capture_stderr) {
    r->err = (char *)calloc (1, 1);
    if (!r->err) { rc = MULIB_EXEC_MEMORY; goto done; }
    if (mulib_process_pipe (errors)) { rc = MULIB_EXEC_SYSTEM; goto done; }
  }
  for (i = 0; i < count; ++i) {
    mulib_exec_command_t c = commands[i];
    if (i + 1 < count && mulib_process_pipe (link)) { rc = MULIB_EXEC_SYSTEM; goto done; }
    if (previous >= 0) c.io.stdin_fd = previous;
    if (i + 1 < count) c.io.stdout_fd = link[1];
    else if (o->capture_stdout) c.io.stdout_fd = output[1];
    if (o->capture_stderr) c.io.stderr_fd = errors[1];
    rc = mulib_exec_start_ (&c, children + i);
    if (rc) { r->failed_stage = i; goto done; }
    mulib_exec_close_ (&previous); mulib_exec_close_ (&link[1]);
    previous = link[0]; link[0] = -1;
  }
  mulib_exec_close_ (&output[1]); mulib_exec_close_ (&errors[1]);
  (void)pthread_mutex_unlock (&mulib_exec_launch_mutex_); launch_locked = 0;
  /* Read at most one buffer per readiness notification, giving both streams
   * and timeout/reaping checks progress even under unbounded child output. */
  do {
    struct pollfd fds[2];
    int delay = 20;
    active = 0;
    for (i = 0; i < count; ++i) if (children[i].pid) {
      int w = mulib_process_wait (children + i, r->stages + i, 1);
      if (w < 0) { rc = MULIB_EXEC_SYSTEM; goto done; }
      if (w == MULIB_PROCESS_RUNNING) ++active;
    }
    if (!active && output[0] < 0 && errors[0] < 0) break;
    if (o->timeout_ms) {
      uint64_t elapsed;
      if (mulib_perf_now (&now)) { rc = MULIB_EXEC_SYSTEM; goto done; }
      elapsed = (now - begin) / 1000000;
      if (elapsed >= o->timeout_ms) { rc = MULIB_EXEC_TIMEOUT; goto done; }
      if (o->timeout_ms - elapsed < (uint64_t)delay) delay = (int)(o->timeout_ms - elapsed);
    }
    fds[0].fd = output[0]; fds[1].fd = errors[0];
    fds[0].events = fds[1].events = POLLIN; fds[0].revents = fds[1].revents = 0;
    if (poll (fds, 2, delay) < 0) {
      if (errno == EINTR) continue;
      rc = MULIB_EXEC_SYSTEM; goto done;
    }
    if (fds[0].revents) rc = mulib_exec_capture_ (&output[0], &r->out, &r->out_size, max);
    if (!rc && fds[1].revents) rc = mulib_exec_capture_ (&errors[0], &r->err, &r->err_size, max);
    if (rc) goto done;
  } while (active || output[0] >= 0 || errors[0] >= 0);
done:
  saved = rc == MULIB_EXEC_SYSTEM ? errno : 0;
  mulib_exec_close_ (&previous); mulib_exec_close_ (&link[0]); mulib_exec_close_ (&link[1]);
  mulib_exec_close_ (&output[0]); mulib_exec_close_ (&output[1]);
  mulib_exec_close_ (&errors[0]); mulib_exec_close_ (&errors[1]);
  if (input) fclose (input);
  if (launch_locked) (void)pthread_mutex_unlock (&mulib_exec_launch_mutex_);
  if (children) {
    /* Signal every child first; no sequential waits on a still-running peer. */
    for (i = 0; i < count; ++i) if (children[i].pid) {
      int w = mulib_process_wait (children + i, r->stages + i, 1);
      if (w == MULIB_PROCESS_RUNNING) (void)kill ((pid_t)children[i].pid, SIGKILL);
    }
    for (i = 0; i < count; ++i) if (children[i].pid)
      (void)mulib_process_wait (children + i, r->stages + i, 0);
  }
  free (children);
  if (r->stages) for (i = 0; i < count; ++i) {
    int code = r->stages[i].exited ? r->stages[i].exit_code :
               r->stages[i].term_signal ? 128 + r->stages[i].term_signal : 0;
    if (!o->pipefail || code) r->code = code;
  }
  r->system_error = saved;
  if (saved) errno = saved;
  return rc;
}
#else
int mulib_exec_start (const mulib_exec_command_t *c, mulib_process_t *p)
{ (void)c; (void)p; return MULIB_EXEC_UNSUPPORTED; }
int mulib_exec_signal (mulib_process_t *p, int sig, mulib_process_status_t *s)
{ (void)p; (void)sig; (void)s; return MULIB_EXEC_UNSUPPORTED; }
int mulib_exec_pipeline (const mulib_exec_command_t *c, size_t n,
                         const mulib_exec_options_t *o, mulib_exec_result_t *r)
{ (void)c; (void)n; (void)o; (void)r; return MULIB_EXEC_UNSUPPORTED; }
#endif
int
mulib_exec_run (const mulib_exec_command_t *c, const mulib_exec_options_t *o, mulib_exec_result_t *r)
{ return mulib_exec_pipeline (c, 1, o, r); }
int
mulib_exec_run_file (const char *path, const mulib_exec_command_t *settings,
                     const mulib_exec_options_t *o, mulib_exec_result_t *r)
{
  mulib_exec_command_t c;
  const char *argv[1];
  if (!path || !*path) return MULIB_EXEC_INVALID;
  if (settings) c = *settings; else mulib_exec_command_init (&c);
  argv[0] = path; c.file = path;
  if (!c.argv && !c.argc) { c.argv = argv; c.argc = 1; }
  return mulib_exec_run (&c, o, r);
}
int
mulib_exec_run_text (const char *text, const mulib_exec_command_t *settings,
                     const mulib_exec_options_t *o, mulib_exec_result_t *r)
{
  mulib_exec_command_t c;
  const char *argv[3];
  if (!text) return MULIB_EXEC_INVALID;
  if (settings) c = *settings; else mulib_exec_command_init (&c);
  argv[0] = "/bin/sh"; argv[1] = "-c"; argv[2] = text;
  c.file = argv[0]; c.argv = argv; c.argc = 3;
  return mulib_exec_run (&c, o, r);
}

/* Public Mune API only: arguments remain rooted during the native callback. */
static mulib_mune_value_t
mulib_exec_field_ (mulib_mune_state_t *s, mulib_mune_value_t t, const char *name)
{
  mulib_mune_value_t k, v = {MULIB_MUNE_NIL, {0}};
  if (!mulib_mune_make_string (s, name, strlen (name), &k)) (void)mulib_mune_raw_get (s, t, k, &v);
  return v;
}
static int
mulib_exec_set_ (mulib_mune_state_t *s, mulib_mune_value_t t, const char *name, mulib_mune_value_t v)
{
  mulib_mune_value_t k;
  return !mulib_mune_make_string (s, name, strlen (name), &k) && !mulib_mune_raw_set (s, t, k, v);
}
static int
mulib_exec_number_ (mulib_mune_state_t *s, mulib_mune_value_t t, const char *name, int64_t n)
{
  mulib_mune_value_t v = {MULIB_MUNE_INTEGER, {0}}; v.as.integer = n;
  return mulib_exec_set_ (s, t, name, v);
}
static int
mulib_exec_string_ (mulib_mune_state_t *s, mulib_mune_value_t t, const char *name, const char **out)
{
  size_t n;
  mulib_mune_value_t v = mulib_exec_field_ (s, t, name);
  if (v.type == MULIB_MUNE_NIL) { *out = NULL; return 1; }
  *out = mulib_mune_value_string (v, &n);
  return *out && !memchr (*out, 0, n);
}
static int
mulib_exec_bool_ (mulib_mune_state_t *s, mulib_mune_value_t t, const char *name, int *out)
{
  mulib_mune_value_t v = mulib_exec_field_ (s, t, name);
  if (v.type != MULIB_MUNE_NIL && v.type != MULIB_MUNE_BOOLEAN) return 0;
  *out = v.type == MULIB_MUNE_BOOLEAN && v.as.boolean; return 1;
}
static int
mulib_exec_vector_ (mulib_mune_state_t *s, mulib_mune_value_t t, const char *const **out, size_t *count)
{
  size_t i, n;
  const char **v = NULL;
  if (t.type != MULIB_MUNE_TABLE) return 0;
  /* One allocation with a bounded count; tables must be dense string arrays. */
  for (i = 0; i <= 65536; ++i) {
    mulib_mune_value_t key = {MULIB_MUNE_INTEGER, {0}}, value;
    key.as.integer = (int64_t)i + 1;
    if (mulib_mune_raw_get (s, t, key, &value)) goto fail;
    if (value.type == MULIB_MUNE_NIL) break;
    if (i == 65536 || !mulib_mune_value_string (value, &n)) goto fail;
  }
  v = (const char **)calloc (i + 1, sizeof (*v));
  if (!v) return 0;
  *count = i;
  for (i = 0; i < *count; ++i) {
    mulib_mune_value_t key = {MULIB_MUNE_INTEGER, {0}}, value;
    key.as.integer = (int64_t)i + 1;
    if (mulib_mune_raw_get (s, t, key, &value)) goto fail;
    v[i] = mulib_mune_value_string (value, &n);
    if (!v[i] || memchr (v[i], 0, n)) goto fail;
  }
  *out = v; return 1;
fail:
  free (v); return 0;
}
static mulib_mune_status_t
mulib_exec_native_ (mulib_mune_state_t *s, const mulib_mune_value_t *args,
                     size_t argc, mulib_mune_result_t *out, void *user)
{
  mulib_exec_command_t *commands = NULL;
  mulib_exec_options_t o = {0};
  mulib_exec_result_t r = {0};
  mulib_mune_value_t stages, result, value, opt = {MULIB_MUNE_NIL, {0}};
  size_t n = 0, i;
  int rc;
  mulib_mune_status_t status = MULIB_MUNE_INVALID;
  (void)user;
  if (!argc || argc > 2 || args[0].type != MULIB_MUNE_TABLE) return status;
  if (argc == 2) { opt = args[1]; if (opt.type != MULIB_MUNE_TABLE && opt.type != MULIB_MUNE_NIL) return status; }
  stages = args[0];
  for (n = 0; n <= 256; ++n) {
    mulib_mune_value_t key = {MULIB_MUNE_INTEGER, {0}}; key.as.integer = (int64_t)n + 1;
    if (mulib_mune_raw_get (s, stages, key, &value)) return status;
    if (value.type == MULIB_MUNE_NIL) break;
    if (value.type != MULIB_MUNE_TABLE) return status;
  }
  if (!n || n > 256) return status;
  commands = (mulib_exec_command_t *)calloc (n, sizeof (*commands));
  if (!commands) return MULIB_MUNE_MEMORY;
  for (i = 0; i < n; ++i) {
    mulib_mune_value_t key = {MULIB_MUNE_INTEGER, {0}}, t, env;
    const char *backend;
    mulib_exec_command_t *c = commands + i;
    key.as.integer = (int64_t)i + 1;
    mulib_exec_command_init (c);
    if (mulib_mune_raw_get (s, stages, key, &t)) goto done;
    if (!mulib_exec_vector_ (s, mulib_exec_field_ (s, t, "argv"), &c->argv, &c->argc)) goto done;
    env = mulib_exec_field_ (s, t, "env");
    if (env.type != MULIB_MUNE_NIL && !mulib_exec_vector_ (s, env, &c->env, &c->envc)) goto done;
    if (!mulib_exec_string_ (s,t,"file",&c->file) || !mulib_exec_string_ (s,t,"cwd",&c->cwd)
        || !mulib_exec_string_ (s,t,"stdin",&c->stdin_path) || !mulib_exec_string_ (s,t,"stdout",&c->stdout_path)
        || !mulib_exec_string_ (s,t,"stderr",&c->stderr_path) || !mulib_exec_string_ (s,t,"doc",&c->doc)
        || !mulib_exec_string_ (s,t,"backend",&backend)
        || !mulib_exec_bool_ (s,t,"append_stdout",&c->append_stdout)
        || !mulib_exec_bool_ (s,t,"append_stderr",&c->append_stderr)
        || !mulib_exec_bool_ (s,t,"merge_stderr",&c->merge_stderr)) goto done;
    if (backend) {
      if (!strcmp (backend,"fork")) c->backend = MULIB_EXEC_FORK;
      else if (!strcmp (backend,"spawn")) c->backend = MULIB_EXEC_SPAWN;
      else if (strcmp (backend,"auto")) goto done;
    }
  }
  if (opt.type == MULIB_MUNE_TABLE) {
    value = mulib_exec_field_ (s,opt,"input");
    if (value.type != MULIB_MUNE_NIL) {
      o.input = mulib_mune_value_string (value, &o.input_size);
      if (!o.input) goto done;
    }
    if (!mulib_exec_bool_ (s,opt,"capture_stdout",&o.capture_stdout)
        || !mulib_exec_bool_ (s,opt,"capture_stderr",&o.capture_stderr)
        || !mulib_exec_bool_ (s,opt,"pipefail",&o.pipefail)) goto done;
    value = mulib_exec_field_ (s,opt,"timeout_ms");
    if (value.type != MULIB_MUNE_NIL) {
      if (value.type != MULIB_MUNE_INTEGER || value.as.integer < 0 || (uint64_t)value.as.integer > UINT_MAX) goto done;
      o.timeout_ms = (unsigned)value.as.integer;
    }
    value = mulib_exec_field_ (s,opt,"max_output");
    if (value.type != MULIB_MUNE_NIL) {
      if (value.type != MULIB_MUNE_INTEGER || value.as.integer < 0 || (uint64_t)value.as.integer >= SIZE_MAX) goto done;
      o.max_output = (size_t)value.as.integer;
    }
  }
  r.failed_stage = SIZE_MAX;
  rc = mulib_exec_pipeline (commands,n,&o,&r);
  status = MULIB_MUNE_MEMORY;
  if (mulib_mune_make_table (s,&result) || !mulib_mune_result_push (out,result)) goto done;
  if (!mulib_exec_number_ (s,result,"status",rc) || !mulib_exec_number_ (s,result,"code",r.code)
      || !mulib_exec_number_ (s,result,"system_error",r.system_error)
      || !mulib_exec_number_ (s,result,"failed_stage",r.failed_stage == SIZE_MAX ? 0 : (int64_t)r.failed_stage + 1)) goto done;
  value.type = MULIB_MUNE_BOOLEAN; value.as.boolean = !rc && !r.code;
  if (!mulib_exec_set_ (s,result,"ok",value)) goto done;
  if (mulib_mune_make_string (s,r.out ? r.out : "",r.out_size,&value)
      || !mulib_exec_set_ (s,result,"stdout",value)
      || mulib_mune_make_string (s,r.err ? r.err : "",r.err_size,&value)
      || !mulib_exec_set_ (s,result,"stderr",value)
      || mulib_mune_make_table (s,&stages) || !mulib_exec_set_ (s,result,"stages",stages)) goto done;
  for (i = 0; i < r.count; ++i) {
    mulib_mune_value_t t, key = {MULIB_MUNE_INTEGER, {0}};
    key.as.integer = (int64_t)i + 1;
    if (mulib_mune_make_table (s,&t) || mulib_mune_raw_set (s,stages,key,t)
        || !mulib_exec_number_ (s,t,"exit_code",r.stages[i].exit_code)
        || !mulib_exec_number_ (s,t,"signal",r.stages[i].term_signal)) goto done;
    value.type = MULIB_MUNE_BOOLEAN; value.as.boolean = r.stages[i].exited;
    if (!mulib_exec_set_ (s,t,"exited",value)) goto done;
  }
  status = MULIB_MUNE_OK;
done:
  for (i = 0; i < n; ++i) { free ((void *)commands[i].argv); free ((void *)commands[i].env); }
  free (commands); mulib_exec_result_destroy (&r); return status;
}
mulib_mune_status_t
mulib_exec_bind (mulib_mune_state_t *s, mulib_mune_error_t *error)
{
  static const char source[] =
    "exec = {docs={}}\n"
    "local function array(t,kind) assert(type(t)=='table','dense array required'); local n=#t; "
    "for k,v in pairs(t) do assert(type(k)=='number' and k>=1 and k<=n and k%1==0 "
    "and type(v)==kind,'invalid array entry') end; "
    "for i=1,n do assert(type(t[i])==kind,'missing array entry') end; return n end\n"
    "exec.docs.command = [[command{argv={program,arg,...},doc=string,...} creates an inert command plan. "
    "Every argv element is one literal argument; quoting, globbing, variables and shell operators are never expanded. "
    "Use file to override the executable while retaining argv[1]. cwd changes only the child directory. "
    "env is a dense array of NAME=value strings replacing the entire environment; omit it to inherit. "
    "stdin/stdout/stderr are child-relative file paths; append_stdout/append_stderr select append instead of truncate. "
    "merge_stderr sends stderr to final stdout. backend is auto, fork or spawn; portable spawn rejects cwd. "
    "doc accepts quoted or long-bracket multiline strings and is retained for inspection. Plans are mutable tables; "
    "execution validates types and rejects NUL in OS strings.]]\n"
    "function exec.command(t) assert(type(t)=='table','command requires table'); "
    "assert(array(t.argv,'string')>0,'command requires argv'); "
    "if t.env~=nil then array(t.env,'string') end; "
    "assert(t.doc==nil or type(t.doc)=='string','doc must be a string'); return t end\n"
    "exec.docs.pipeline = [[pipeline(command,...) returns a dense ordered stage array. All stages are started before "
    "waiting, so output larger than a pipe buffer can flow. Only the first stage accepts input redirection and only "
    "the last accepts output redirection. stderr is inherited, redirected per stage or captured together. "
    "One through 256 stages are supported. Creating a pipeline does not execute it.]]\n"
    "function exec.pipeline(...) return {...} end\n"
    "exec.docs.run = [[run(command_or_pipeline,options) executes synchronously and returns a result table. Options: "
    "input is a binary string staged in a temporary regular file, capture_stdout/capture_stderr are booleans, "
    "max_output bounds each capture (default 16777216), timeout_ms is a monotonic deadline (0 disables), "
    "pipefail selects the rightmost nonzero stage status instead of the last stage. Captured stdout/stderr are "
    "binary strings; stages contain exited, exit_code and signal. code is the selected exit status (128+signal "
    "for signals); status is 0, -1 invalid, -2 memory, -3 system, -4 unsupported, -5 timeout or -6 capture limit. "
    "system_error is errno for system failures; failed_stage is a one-based launch index or 0. ok requires "
    "both status and code zero. Nonzero child exits do not throw. Setup errors reap started children; timeouts "
    "kill direct children only. Descendants must be managed by the application. A blocking OS launch/open can "
    "delay deadline observation. Conflicting redirects/capture are errors.]]\n"
    "function exec.run(p,o) if p.argv then p={p} end; array(p,'table'); "
    "for _,c in ipairs(p) do exec.command(c) end; return _mulib_exec_run(p,o) end\n";
  static const char helpers[] =
    "exec.docs.capture = [[capture(plan,options) copies options, enables stdout and stderr capture and calls run. "
    "It returns the complete result, including partial output on failure. It never changes the caller's options. "
    "Use stdout length rather than line splitting for NUL-delimited Git output; binary output is preserved.]]\n"
    "function exec.capture(p,o) local c={}; for k,v in pairs(o or {}) do c[k]=v end; "
    "c.capture_stdout=true; c.capture_stderr=true; return exec.run(p,c) end\n"
    "exec.docs.check = [[check(plan,options) calls run and returns its result if ok. Otherwise raises a Lua error "
    "containing the execution status, selected exit code and stderr. Earlier external side effects persist. "
    "Use run for commands such as diff or grep where exit 1 is an expected predicate result.]]\n"
    "function exec.check(p,o) local r=exec.run(p,o); if not r.ok then "
    "error('command failed: status='..r.status..' code='..r.code..' '..r.stderr) end; return r end\n"
    "exec.docs.sequence = [[sequence(plans,options,keep_going) runs plans in array order, returning results. "
    "By default it stops after the first result with ok=false; keep_going=true runs the rest. Each plan can be "
    "one command or a pipeline, and each receives the same run options. This is conditional sequencing without "
    "shell parsing; use ordinary Lua if/loops/functions for branching, retries and workflow composition.]]\n"
    "function exec.sequence(ps,o,keep) local rs={}; for _,p in ipairs(ps) do "
    "local r=exec.run(p,o); rs[#rs+1]=r; if not r.ok and not keep then break end end; return rs end\n";
  mulib_mune_result_t *r = NULL;
  mulib_mune_status_t rc = mulib_mune_bind (s,"_mulib_exec_run",mulib_exec_native_,NULL);
  if (!rc) rc = mulib_mune_run_source (s,source,sizeof (source)-1,&r,error);
  mulib_mune_result_destroy (r); r = NULL;
  if (!rc) rc = mulib_mune_run_source (s,helpers,sizeof (helpers)-1,&r,error);
  mulib_mune_result_destroy (r); return rc;
}
#endif
