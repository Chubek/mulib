#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "supervisor.h"

#include <string.h>

#define HEX_CHUNK_BYTES 2047u /* The kernel's text argument limit is 4095. */
#define HEX_CHUNKS ((BASIS_DAEMON_MAX_CONTEXT + HEX_CHUNK_BYTES - 1) / HEX_CHUNK_BYTES)

static int
valid_name(const char *s)
{
  size_t i;
  if (!s || !s[0]) return 0;
  for (i = 0; s[i]; ++i) {
    unsigned char c = (unsigned char)s[i];
    if (i >= 127 || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                     || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
      return 0;
  }
  return strcmp(s, ".") && strcmp(s, "..");
}

static int
valid_path(const char *s)
{
  size_t i;
  if (!s || s[0] != '/') return 0;
  for (i = 1; s[i]; ++i)
    if (i >= 4095 || (unsigned char)s[i] < 32 || (unsigned char)s[i] == 127)
      return 0;
  return 1;
}

static const basis_daemon_runner *
find_runner(const basis_daemon_runner *runners, size_t count, const basis_daemon_job *job)
{
  const basis_daemon_runner *found = NULL;
  size_t i, j;
  if (!runners || !count || count > BASIS_DAEMON_MAX_RUNNERS
      || !job || !valid_name(job->runner) || job->context_len > BASIS_DAEMON_MAX_CONTEXT
      || (job->context_len && !job->context))
    return NULL;
  for (i = 0; i < count; ++i) {
    if (!valid_name(runners[i].name) || !valid_path(runners[i].executable)
        || !runners[i].executable[1] || !valid_path(runners[i].directory))
      return NULL;
    for (j = 0; j < i; ++j)
      if (!strcmp(runners[j].name, runners[i].name))
        return NULL;
    if (!strcmp(runners[i].name, job->runner))
      found = &runners[i];
  }
  return found;
}

basis_daemon_status
basis_daemon_validate_job(const basis_daemon_runner *runners, size_t runner_count,
                          const basis_daemon_job *job)
{
  return find_runner(runners, runner_count, job) ? BASIS_DAEMON_OK : BASIS_DAEMON_INVALID;
}

static int
hex_value(unsigned char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

basis_daemon_status
basis_daemon_decode_context(int argc, const char *const *argv, const char *expected_runner,
                            unsigned char *output, size_t capacity, size_t *length)
{
  size_t total = 0, digits[HEX_CHUNKS];
  int i;
  if (!argv || argc < 3 || argc > (int)(2 + HEX_CHUNKS)
      || !valid_name(expected_runner) || !valid_name(argv[1])
      || strcmp(argv[1], expected_runner) || !length)
    return BASIS_DAEMON_INVALID;
  for (i = 2; i < argc; ++i) {
    size_t n;
    if (!argv[i]) return BASIS_DAEMON_INVALID;
    for (n = 0; argv[i][n]; ++n)
      if (n >= HEX_CHUNK_BYTES * 2 || hex_value((unsigned char)argv[i][n]) < 0)
        return BASIS_DAEMON_INVALID;
    if (n % 2 || (i < argc - 1 && n != HEX_CHUNK_BYTES * 2)
        || (i > 2 && n == 0) || n / 2 > BASIS_DAEMON_MAX_CONTEXT - total)
      return BASIS_DAEMON_INVALID;
    digits[i - 2] = n;
    total += n / 2;
  }
  if (total > capacity || (total && !output)) return BASIS_DAEMON_INVALID;
  total = 0;
  for (i = 2; i < argc; ++i) {
    size_t n;
    for (n = 0; n < digits[i - 2]; n += 2)
      output[total++] = (unsigned char)(16 * hex_value((unsigned char)argv[i][n])
                                        + hex_value((unsigned char)argv[i][n + 1]));
  }
  *length = total;
  return BASIS_DAEMON_OK;
}

static void
set_event(basis_daemon_event *event, basis_daemon_state state, int error, int owned)
{
  if (event) *event = (basis_daemon_event){state, -1, 0, error, owned};
}

static int
valid_ms(unsigned ms)
{
  return ms > 0 && ms <= BASIS_DAEMON_MAX_MS;
}

basis_daemon_status
basis_daemon_validate_limits(const basis_daemon_limits *limits)
{
  return limits && valid_ms(limits->startup_ms) && valid_ms(limits->run_ms)
         && valid_ms(limits->reap_ms) && limits->grace_ms <= BASIS_DAEMON_MAX_MS
         ? BASIS_DAEMON_OK : BASIS_DAEMON_INVALID;
}

basis_daemon_status
basis_daemon_validate_policy(const basis_daemon_policy *policy)
{
  if (!policy || basis_daemon_validate_limits(&policy->limits)) return BASIS_DAEMON_INVALID;
  switch (policy->restart) {
  case BASIS_DAEMON_RESTART_NEVER:
    return !policy->max_restarts && !policy->backoff_ms ? BASIS_DAEMON_OK : BASIS_DAEMON_INVALID;
  case BASIS_DAEMON_RESTART_ON_FAILURE:
  case BASIS_DAEMON_RESTART_ALWAYS:
    return policy->max_restarts > 0 && policy->max_restarts <= BASIS_DAEMON_MAX_RESTARTS
           && valid_ms(policy->backoff_ms) ? BASIS_DAEMON_OK : BASIS_DAEMON_INVALID;
  }
  return BASIS_DAEMON_INVALID;
}

static void
queue_event_init(basis_daemon_queue_event *event)
{
  if (!event) return;
  *event = (basis_daemon_queue_event){0};
  event->process.exit_code = -1;
}

#if defined(__linux__) && !defined(MULIB_DAEMON_NO_POSIX) && !defined(MULIB_PROCESS_NO_POSIX)

#include "mulib-daemon.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct basis_daemon_supervisor {
  mulib_process_t child;
  pid_t creator;
  basis_daemon_event last;
};

static void
init_supervisor(basis_daemon_supervisor *s)
{
  mulib_process_init(&s->child);
  s->creator = getpid();
  set_event(&s->last, BASIS_DAEMON_IDLE, 0, 0);
}

basis_daemon_status
basis_daemon_create_supervisor(basis_daemon_supervisor **owner)
{
  basis_daemon_supervisor *s;
  if (!owner || *owner) return BASIS_DAEMON_INVALID;
  s = malloc(sizeof *s);
  if (!s) return BASIS_DAEMON_NOMEM;
  init_supervisor(s);
  *owner = s;
  return BASIS_DAEMON_OK;
}

basis_daemon_status
basis_daemon_destroy_supervisor(basis_daemon_supervisor **owner)
{
  if (!owner || !*owner) return BASIS_DAEMON_INVALID;
  if ((*owner)->creator != getpid()) return BASIS_DAEMON_OWNERSHIP;
  if ((*owner)->child.pid) return BASIS_DAEMON_BUSY;
  free(*owner);
  *owner = NULL;
  return BASIS_DAEMON_OK;
}

static basis_daemon_status
check_owner(basis_daemon_supervisor *owner, basis_daemon_event *event)
{
  set_event(event, BASIS_DAEMON_FAILED, 0, 0);
  if (!owner || !event) return BASIS_DAEMON_INVALID;
  if (owner->creator != getpid()) return BASIS_DAEMON_OWNERSHIP;
  event->child_owned = owner->child.pid != 0;
  return BASIS_DAEMON_OK;
}

static void
save_terminal(basis_daemon_supervisor *s, const basis_daemon_event *event)
{
  if (!event->child_owned && event->state != BASIS_DAEMON_IDLE)
    s->last = *event;
}

static basis_daemon_status
build(const basis_daemon_runner *runner, const basis_daemon_job *job, mulib_daemon_t **out)
{
  static const char hex[] = "0123456789abcdef";
  char chunks[HEX_CHUNKS][HEX_CHUNK_BYTES * 2 + 1];
  const char *argv[2 + HEX_CHUNKS] = {runner->executable, runner->name};
  size_t offset = 0, argc = 2;
  mulib_daemon_t *spec;
  int rc;
  do {
    size_t n = job->context_len - offset, i;
    char *chunk = chunks[argc - 2];
    if (n > HEX_CHUNK_BYTES) n = HEX_CHUNK_BYTES;
    for (i = 0; i < n; ++i) {
      unsigned char b = job->context[offset + i];
      chunk[2 * i] = hex[b >> 4];
      chunk[2 * i + 1] = hex[b & 15];
    }
    chunk[n * 2] = '\0';
    argv[argc++] = chunk;
    offset += n;
  } while (offset < job->context_len);

  spec = mulib_daemon_create(runner->name);
  if (!spec) return BASIS_DAEMON_NOMEM; /* Name has already been validated. */
  rc = mulib_daemon_directory(spec, runner->directory);
  if (!rc) rc = mulib_daemon_command(spec, argv, argc);
  if (rc) {
    mulib_daemon_destroy(spec);
    return rc == MULIB_DAEMON_MEMORY ? BASIS_DAEMON_NOMEM : BASIS_DAEMON_INVALID;
  }
  *out = spec;
  return BASIS_DAEMON_OK;
}

/* Elapsed arithmetic uses differences of monotonic samples, never an absolute
 * time_t addition. Clock jumps outside our small window saturate the wait. */
static int
remaining_ms(const struct timespec *start, unsigned budget)
{
  struct timespec now;
  long s, ns, elapsed_ms;
  if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
  if (now.tv_sec < start->tv_sec || now.tv_sec - start->tv_sec > 61)
    return 0;
  s = (long)(now.tv_sec - start->tv_sec);
  ns = now.tv_nsec - start->tv_nsec;
  if (ns < 0) {
    --s;
    ns += 1000000000L;
  }
  if (s < 0) return 0;
  elapsed_ms = s * 1000 + ns / 1000000;
  return elapsed_ms < (long)budget ? (int)((long)budget - elapsed_ms) : 0;
}

static basis_daemon_status
poll_child(basis_daemon_supervisor *s, basis_daemon_event *event)
{
  mulib_process_status_t result;
  long pid = s->child.pid;
  int rc, saved;
  if (!pid) {
    set_event(event, BASIS_DAEMON_IDLE, 0, 0);
    return BASIS_DAEMON_OK;
  }
  rc = mulib_process_wait(&s->child, &result, 1);
  saved = errno;
  if (rc == MULIB_PROCESS_RUNNING) {
    set_event(event, BASIS_DAEMON_RUNNING, 0, 1);
    return BASIS_DAEMON_OK;
  }
  if (rc == MULIB_PROCESS_OK) {
    if (!result.exited && !result.term_signal) {
      /* Linux waitpid can report a traced child's stop without WUNTRACED.
       * The kernel wrapper cleared the handle, but no child was reaped. */
      s->child.pid = pid;
      set_event(event, BASIS_DAEMON_RUNNING, 0, 1);
      return BASIS_DAEMON_OK;
    }
    set_event(event, result.exited ? BASIS_DAEMON_EXITED : BASIS_DAEMON_SIGNALED, 0, 0);
    if (result.exited) event->exit_code = result.exit_code;
    else event->term_signal = result.term_signal;
    return BASIS_DAEMON_OK;
  }
  set_event(event, BASIS_DAEMON_FAILED, rc == MULIB_PROCESS_SYSTEM ? saved : 0,
            s->child.pid != 0);
  if (rc == MULIB_PROCESS_SYSTEM && saved == ECHILD) return BASIS_DAEMON_OWNERSHIP;
  if (rc == MULIB_PROCESS_UNSUPPORTED) return BASIS_DAEMON_UNSUPPORTED;
  return BASIS_DAEMON_SYSTEM;
}

static basis_daemon_status
wait_child(basis_daemon_supervisor *s, unsigned budget, basis_daemon_event *event)
{
  struct timespec start;
  int remaining;
  basis_daemon_status rc;
  if (clock_gettime(CLOCK_MONOTONIC, &start)) goto error;
  for (;;) {
    rc = poll_child(s, event);
    if (rc || event->state != BASIS_DAEMON_RUNNING) return rc;
    remaining = remaining_ms(&start, budget);
    if (remaining < 0) goto error;
    if (!remaining) return BASIS_DAEMON_OK;
    /* Small fixed slices avoid a global SIGCHLD handler or a busy spin. */
    if (poll(NULL, 0, remaining < 5 ? remaining : 5) < 0 && errno != EINTR)
      goto error;
  }
error:
  set_event(event, BASIS_DAEMON_FAILED, errno, s->child.pid != 0);
  return BASIS_DAEMON_SYSTEM;
}

static basis_daemon_status
signal_child(basis_daemon_supervisor *s, int signum, basis_daemon_event *event)
{
  basis_daemon_status rc = poll_child(s, event);
  if (rc || event->state != BASIS_DAEMON_RUNNING) return rc;
  /* The exclusive reaper owns the unreaped child, so its pid cannot be reused.
   * In contrast, mulib_daemon_stop can reap while discarding exit status and
   * mulib_process_terminate does a blocking wait, so neither is used here. */
  if (kill((pid_t)s->child.pid, signum) && errno != ESRCH) {
    set_event(event, BASIS_DAEMON_FAILED, errno, 1);
    return BASIS_DAEMON_SYSTEM;
  }
  return BASIS_DAEMON_OK;
}

static basis_daemon_status
stop_child(basis_daemon_supervisor *s, unsigned grace_ms, unsigned reap_ms,
           basis_daemon_event *event)
{
  basis_daemon_status rc;
  if (grace_ms) {
    rc = signal_child(s, SIGTERM, event);
    if (rc || event->state != BASIS_DAEMON_RUNNING) return rc;
    rc = wait_child(s, grace_ms, event);
    if (rc) return rc;
    if (event->state != BASIS_DAEMON_RUNNING) {
      event->state = BASIS_DAEMON_STOPPED;
      return BASIS_DAEMON_OK;
    }
  }
  rc = signal_child(s, SIGKILL, event);
  if (rc) return rc;
  if (event->state != BASIS_DAEMON_RUNNING) {
    if (grace_ms) event->state = BASIS_DAEMON_STOPPED;
    return BASIS_DAEMON_OK;
  }
  rc = wait_child(s, reap_ms, event);
  if (rc) return rc;
  if (event->state == BASIS_DAEMON_RUNNING) {
    event->state = BASIS_DAEMON_STOP_TIMEOUT;
    return BASIS_DAEMON_TIMEOUT;
  }
  event->state = BASIS_DAEMON_STOPPED;
  return BASIS_DAEMON_OK;
}

/* This is the kernel launcher's close-on-exec acknowledgement, not an
 * application readiness protocol. */
static basis_daemon_status
await_exec(int fd, const struct timespec *start, unsigned budget, int *system_error)
{
  struct pollfd pfd = {.fd = fd, .events = POLLIN};
  for (;;) {
    int rc, remaining = remaining_ms(start, budget);
    if (remaining < 0) break;
    if (!remaining) return BASIS_DAEMON_TIMEOUT;
    rc = poll(&pfd, 1, remaining);
    if (rc < 0 && errno == EINTR) continue;
    if (rc < 0) break;
    if (!rc) return BASIS_DAEMON_TIMEOUT;
    if (pfd.revents & POLLNVAL) {
      errno = EBADF;
      break;
    }
    rc = mulib_daemon_launch_status(fd);
    if (rc == MULIB_DAEMON_START_PENDING) continue;
    if (rc == MULIB_DAEMON_OK) return BASIS_DAEMON_OK;
    if (rc != MULIB_DAEMON_SYSTEM) errno = EIO;
    break;
  }
  *system_error = errno;
  return BASIS_DAEMON_SYSTEM;
}

static basis_daemon_status
launch(basis_daemon_supervisor *s, const mulib_daemon_t *spec, unsigned startup_ms,
       unsigned reap_ms, basis_daemon_event *event)
{
  struct sigaction child_action;
  struct timespec start;
  int fd, krc, system_error = 0;
  basis_daemon_status rc;
  set_event(event, BASIS_DAEMON_START_FAILED, 0, 0);
  if (sigaction(SIGCHLD, NULL, &child_action) || clock_gettime(CLOCK_MONOTONIC, &start)) {
    event->system_error = errno;
    return BASIS_DAEMON_SYSTEM;
  }
  if (child_action.sa_handler != SIG_DFL || (child_action.sa_flags & SA_NOCLDWAIT))
    return BASIS_DAEMON_OWNERSHIP;
  krc = mulib_daemon_launch_begin(spec, &s->child, &fd);
  if (krc) {
    if (krc == MULIB_DAEMON_SYSTEM) event->system_error = errno;
    return krc == MULIB_DAEMON_UNSUPPORTED ? BASIS_DAEMON_UNSUPPORTED : BASIS_DAEMON_SYSTEM;
  }
  rc = await_exec(fd, &start, startup_ms, &system_error);
  (void)mulib_process_close(fd);
  if (!rc) return poll_child(s, event);
  {
    basis_daemon_event cleanup;
    basis_daemon_status stopped = stop_child(s, 0, reap_ms, &cleanup);
    *event = cleanup;
    event->state = rc == BASIS_DAEMON_TIMEOUT ? BASIS_DAEMON_START_TIMEOUT : BASIS_DAEMON_START_FAILED;
    if (system_error) event->system_error = system_error;
    return stopped ? stopped : rc;
  }
}

basis_daemon_status
basis_daemon_start(basis_daemon_supervisor *owner, const basis_daemon_runner *runners,
                   size_t runner_count, const basis_daemon_job *job, unsigned startup_ms,
                   unsigned reap_ms, basis_daemon_event *event)
{
  const basis_daemon_runner *runner;
  mulib_daemon_t *spec;
  basis_daemon_status rc = check_owner(owner, event);
  if (rc) return rc;
  if (!valid_ms(startup_ms) || !valid_ms(reap_ms)) return BASIS_DAEMON_INVALID;
  if (owner->child.pid) return BASIS_DAEMON_BUSY;
  runner = find_runner(runners, runner_count, job);
  if (!runner) return BASIS_DAEMON_INVALID;
  rc = build(runner, job, &spec);
  if (rc) return rc;
  rc = launch(owner, spec, startup_ms, reap_ms, event);
  mulib_daemon_destroy(spec);
  save_terminal(owner, event);
  return rc;
}

basis_daemon_status
basis_daemon_poll(basis_daemon_supervisor *owner, basis_daemon_event *event)
{
  basis_daemon_status rc = check_owner(owner, event);
  if (rc) return rc;
  if (!owner->child.pid) {
    *event = owner->last;
    return BASIS_DAEMON_OK;
  }
  rc = poll_child(owner, event);
  save_terminal(owner, event);
  return rc;
}

basis_daemon_status
basis_daemon_wait(basis_daemon_supervisor *owner, unsigned wait_ms, basis_daemon_event *event)
{
  basis_daemon_status rc = check_owner(owner, event);
  if (rc) return rc;
  if (!valid_ms(wait_ms)) return BASIS_DAEMON_INVALID;
  if (!owner->child.pid) {
    *event = owner->last;
    return BASIS_DAEMON_OK;
  }
  rc = wait_child(owner, wait_ms, event);
  save_terminal(owner, event);
  return rc;
}

basis_daemon_status
basis_daemon_stop(basis_daemon_supervisor *owner, unsigned grace_ms, unsigned reap_ms,
                  basis_daemon_event *event)
{
  basis_daemon_status rc = check_owner(owner, event);
  if (rc) return rc;
  if (grace_ms > BASIS_DAEMON_MAX_MS || !valid_ms(reap_ms)) return BASIS_DAEMON_INVALID;
  rc = stop_child(owner, grace_ms, reap_ms, event);
  save_terminal(owner, event);
  return rc;
}

basis_daemon_status
basis_daemon_restart(basis_daemon_supervisor *owner, const basis_daemon_runner *runners,
                     size_t runner_count, const basis_daemon_job *job,
                     const basis_daemon_limits *limits, basis_daemon_event *event)
{
  const basis_daemon_runner *runner;
  mulib_daemon_t *spec;
  basis_daemon_status rc = check_owner(owner, event);
  if (rc) return rc;
  if (basis_daemon_validate_limits(limits)) return BASIS_DAEMON_INVALID;
  runner = find_runner(runners, runner_count, job);
  if (!runner) return BASIS_DAEMON_INVALID;
  rc = build(runner, job, &spec);
  if (rc) return rc;
  rc = stop_child(owner, limits->grace_ms, limits->reap_ms, event);
  if (!rc) rc = launch(owner, spec, limits->startup_ms, limits->reap_ms, event);
  mulib_daemon_destroy(spec);
  save_terminal(owner, event);
  return rc;
}

basis_daemon_status
basis_daemon_run(basis_daemon_supervisor *owner, const basis_daemon_runner *runners,
                 size_t runner_count, const basis_daemon_job *job,
                 const basis_daemon_limits *limits, basis_daemon_event *event)
{
  basis_daemon_status rc;
  if (basis_daemon_validate_limits(limits)) {
    rc = check_owner(owner, event);
    return rc ? rc : BASIS_DAEMON_INVALID;
  }
  rc = basis_daemon_start(owner, runners, runner_count, job, limits->startup_ms,
                          limits->reap_ms, event);
  if (rc || event->state != BASIS_DAEMON_RUNNING) return rc;
  rc = wait_child(owner, limits->run_ms, event);
  if (rc && event->child_owned) {
    basis_daemon_event cleanup;
    (void)stop_child(owner, 0, limits->reap_ms, &cleanup);
    event->child_owned = cleanup.child_owned;
  }
  if (rc || event->state != BASIS_DAEMON_RUNNING) {
    save_terminal(owner, event);
    return rc;
  }
  rc = stop_child(owner, limits->grace_ms, limits->reap_ms, event);
  event->state = BASIS_DAEMON_RUN_TIMEOUT;
  save_terminal(owner, event);
  return rc ? rc : BASIS_DAEMON_TIMEOUT;
}

typedef struct {
  mulib_daemon_t *spec;
  basis_daemon_policy policy;
  uint64_t id;
  unsigned attempt;
} queue_job;

enum queue_phase { QUEUE_NEW, QUEUE_RUN, QUEUE_RETRY, QUEUE_REAP, QUEUE_CANCEL };

struct basis_daemon_queue {
  basis_daemon_supervisor worker;
  queue_job active, waiting[BASIS_DAEMON_MAX_PENDING];
  size_t count, capacity;
  uint64_t next_id;
  int closed;
  enum queue_phase phase;
  struct timespec mark; /* Start of a run budget or a backoff. */
  basis_daemon_status last_status;
  basis_daemon_event last_event;
};

basis_daemon_status
basis_daemon_queue_create(size_t capacity, basis_daemon_queue **out)
{
  basis_daemon_queue *q;
  if (!out || *out || !capacity || capacity > BASIS_DAEMON_MAX_PENDING)
    return BASIS_DAEMON_INVALID;
  q = calloc(1, sizeof *q);
  if (!q) return BASIS_DAEMON_NOMEM;
  init_supervisor(&q->worker);
  q->next_id = 1;
  q->capacity = capacity;
  *out = q;
  return BASIS_DAEMON_OK;
}

static basis_daemon_status
check_queue(const basis_daemon_queue *q)
{
  if (!q) return BASIS_DAEMON_INVALID;
  return q->worker.creator == getpid() ? BASIS_DAEMON_OK : BASIS_DAEMON_OWNERSHIP;
}

basis_daemon_status
basis_daemon_queue_destroy(basis_daemon_queue **out)
{
  basis_daemon_status rc = check_queue(out ? *out : NULL);
  if (rc) return rc;
  if ((*out)->active.id || (*out)->count || (*out)->worker.child.pid) return BASIS_DAEMON_BUSY;
  free(*out);
  *out = NULL;
  return BASIS_DAEMON_OK;
}

basis_daemon_status
basis_daemon_queue_submit(basis_daemon_queue *q, const basis_daemon_runner *runners,
                          size_t runner_count, const basis_daemon_job *job,
                          const basis_daemon_policy *policy, uint64_t *id)
{
  const basis_daemon_runner *runner;
  queue_job next = {0};
  basis_daemon_status rc = check_queue(q);
  if (rc) return rc;
  if (!id || basis_daemon_validate_policy(policy)) return BASIS_DAEMON_INVALID;
  if (q->closed) return BASIS_DAEMON_CLOSED;
  runner = find_runner(runners, runner_count, job);
  if (!runner) return BASIS_DAEMON_INVALID;
  if (q->count == q->capacity || !q->next_id) return BASIS_DAEMON_LIMIT;
  rc = build(runner, job, &next.spec);
  if (rc) return rc;
  next.policy = *policy;
  next.id = q->next_id;
  q->next_id = q->next_id == UINT64_MAX ? 0 : q->next_id + 1;
  q->waiting[q->count++] = next;
  *id = next.id;
  return BASIS_DAEMON_OK;
}

static queue_job
remove_waiting(basis_daemon_queue *q, size_t index)
{
  queue_job item = q->waiting[index];
  --q->count;
  if (index < q->count)
    memmove(q->waiting + index, q->waiting + index + 1, (q->count - index) * sizeof item);
  q->waiting[q->count] = (queue_job){0};
  return item;
}

static void
queue_report(const basis_daemon_queue *q, basis_daemon_queue_state state, basis_daemon_queue_event *out)
{
  out->state = state;
  out->id = q->active.id;
  out->attempt = q->active.attempt;
  out->pending = q->count;
  out->process_status = q->last_status;
  out->process = q->last_event;
}

static void
release_active(basis_daemon_queue *q)
{
  mulib_daemon_destroy(q->active.spec);
  q->active = (queue_job){0};
}

static void
queue_terminal(basis_daemon_queue *q, basis_daemon_queue_event *out)
{
  const basis_daemon_policy *p = &q->active.policy;
  int exited = q->last_status == BASIS_DAEMON_OK && q->last_event.state == BASIS_DAEMON_EXITED;
  int retryable = exited
                  || (q->last_status == BASIS_DAEMON_OK && q->last_event.state == BASIS_DAEMON_SIGNALED)
                  || (q->last_status == BASIS_DAEMON_SYSTEM && q->last_event.state == BASIS_DAEMON_START_FAILED)
                  || (q->last_status == BASIS_DAEMON_TIMEOUT
                      && (q->last_event.state == BASIS_DAEMON_START_TIMEOUT
                          || q->last_event.state == BASIS_DAEMON_RUN_TIMEOUT));
  if (q->last_event.child_owned) {
    q->phase = QUEUE_REAP;
    queue_report(q, BASIS_DAEMON_QUEUE_CLEANUP, out);
    return;
  }
  if (retryable && q->active.attempt <= p->max_restarts
      && (p->restart == BASIS_DAEMON_RESTART_ALWAYS
          || (p->restart == BASIS_DAEMON_RESTART_ON_FAILURE && !(exited && !q->last_event.exit_code)))) {
    if (!clock_gettime(CLOCK_MONOTONIC, &q->mark)) {
      q->phase = QUEUE_RETRY;
      queue_report(q, BASIS_DAEMON_QUEUE_BACKOFF, out);
      return;
    }
    set_event(&q->last_event, BASIS_DAEMON_FAILED, errno, 0);
    q->last_status = BASIS_DAEMON_SYSTEM;
  }
  queue_report(q, BASIS_DAEMON_QUEUE_FINISHED, out);
  release_active(q);
}

static void
queue_start_attempt(basis_daemon_queue *q, basis_daemon_queue_event *out)
{
  const basis_daemon_limits *lim = &q->active.policy.limits;
  ++q->active.attempt;
  q->last_status = launch(&q->worker, q->active.spec, lim->startup_ms, lim->reap_ms, &q->last_event);
  if (q->last_status || q->last_event.state != BASIS_DAEMON_RUNNING) {
    queue_terminal(q, out);
    return;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &q->mark)) {
    set_event(&q->last_event, BASIS_DAEMON_FAILED, errno, 1);
    q->last_status = BASIS_DAEMON_SYSTEM;
    queue_terminal(q, out);
    return;
  }
  q->phase = QUEUE_RUN;
  queue_report(q, BASIS_DAEMON_QUEUE_RUNNING, out);
}

static basis_daemon_status
queue_cancel_active(basis_daemon_queue *q, basis_daemon_queue_event *out)
{
  const basis_daemon_limits *lim = &q->active.policy.limits;
  unsigned grace = q->phase == QUEUE_CANCEL || q->phase == QUEUE_REAP ? 0 : lim->grace_ms;
  q->phase = QUEUE_CANCEL;
  q->last_status = stop_child(&q->worker, grace, lim->reap_ms, &q->last_event);
  if (q->last_event.child_owned)
    queue_report(q, BASIS_DAEMON_QUEUE_CLEANUP, out);
  else {
    queue_report(q, BASIS_DAEMON_QUEUE_CANCELLED, out);
    release_active(q);
  }
  return q->last_status;
}

basis_daemon_status
basis_daemon_queue_step(basis_daemon_queue *q, basis_daemon_queue_event *out)
{
  const basis_daemon_limits *lim;
  int remaining;
  basis_daemon_status rc = check_queue(q);
  queue_event_init(out);
  if (rc || !out) return rc ? rc : BASIS_DAEMON_INVALID;
  if (!q->active.id) {
    if (!q->count) return BASIS_DAEMON_OK;
    q->active = remove_waiting(q, 0);
    q->phase = QUEUE_NEW;
  }
  lim = &q->active.policy.limits;
  switch (q->phase) {
  case QUEUE_NEW:
    queue_start_attempt(q, out);
    break;
  case QUEUE_RUN:
    q->last_status = poll_child(&q->worker, &q->last_event);
    if (!q->last_status && q->last_event.state == BASIS_DAEMON_RUNNING) {
      remaining = remaining_ms(&q->mark, lim->run_ms);
      if (remaining > 0) {
        queue_report(q, BASIS_DAEMON_QUEUE_RUNNING, out);
        break;
      }
      if (remaining < 0) {
        set_event(&q->last_event, BASIS_DAEMON_FAILED, errno, 1);
        q->last_status = BASIS_DAEMON_SYSTEM;
      } else {
        q->last_status = stop_child(&q->worker, lim->grace_ms, lim->reap_ms, &q->last_event);
        if (!q->last_status) q->last_status = BASIS_DAEMON_TIMEOUT;
        if (q->last_status == BASIS_DAEMON_TIMEOUT) q->last_event.state = BASIS_DAEMON_RUN_TIMEOUT;
      }
    }
    queue_terminal(q, out);
    break;
  case QUEUE_RETRY:
    remaining = remaining_ms(&q->mark, q->active.policy.backoff_ms);
    if (remaining > 0) queue_report(q, BASIS_DAEMON_QUEUE_BACKOFF, out);
    else if (!remaining) queue_start_attempt(q, out);
    else {
      set_event(&q->last_event, BASIS_DAEMON_FAILED, errno, 0);
      q->last_status = BASIS_DAEMON_SYSTEM;
      queue_terminal(q, out);
    }
    break;
  case QUEUE_REAP: {
    basis_daemon_event cleanup;
    rc = stop_child(&q->worker, 0, lim->reap_ms, &cleanup);
    if (rc) {
      if (!cleanup.child_owned) {
        q->last_event = cleanup;
        q->last_status = rc;
      } else {
        queue_report(q, BASIS_DAEMON_QUEUE_CLEANUP, out);
        out->process = cleanup;
        out->process_status = rc;
        break;
      }
    } else {
      q->last_event.child_owned = cleanup.child_owned;
      q->last_event.exit_code = cleanup.exit_code;
      q->last_event.term_signal = cleanup.term_signal;
    }
    queue_terminal(q, out);
    break;
  }
  case QUEUE_CANCEL:
    (void)queue_cancel_active(q, out);
    break;
  }
  return BASIS_DAEMON_OK;
}

basis_daemon_status
basis_daemon_queue_cancel(basis_daemon_queue *q, uint64_t id, basis_daemon_queue_event *out)
{
  size_t i;
  basis_daemon_status rc = check_queue(q);
  queue_event_init(out);
  if (rc || !out || !id) return rc ? rc : BASIS_DAEMON_INVALID;
  if (q->active.id == id) return queue_cancel_active(q, out);
  for (i = 0; i < q->count; ++i) {
    if (q->waiting[i].id == id) {
      queue_job item = remove_waiting(q, i);
      mulib_daemon_destroy(item.spec);
      out->state = BASIS_DAEMON_QUEUE_CANCELLED;
      out->id = item.id;
      out->pending = q->count;
      return BASIS_DAEMON_OK;
    }
  }
  out->pending = q->count;
  return BASIS_DAEMON_NOT_FOUND;
}

basis_daemon_status
basis_daemon_queue_shutdown(basis_daemon_queue *q, basis_daemon_queue_event *out)
{
  size_t n;
  basis_daemon_status rc = check_queue(q);
  queue_event_init(out);
  if (rc || !out) return rc ? rc : BASIS_DAEMON_INVALID;
  q->closed = 1;
  n = q->count;
  while (q->count) {
    queue_job item = remove_waiting(q, q->count - 1);
    mulib_daemon_destroy(item.spec);
  }
  if (q->active.id) rc = queue_cancel_active(q, out);
  out->dropped = n;
  return rc;
}

#else

basis_daemon_status
basis_daemon_create_supervisor(basis_daemon_supervisor **owner)
{
  return !owner || *owner ? BASIS_DAEMON_INVALID : BASIS_DAEMON_UNSUPPORTED;
}

basis_daemon_status
basis_daemon_destroy_supervisor(basis_daemon_supervisor **owner)
{
  return !owner ? BASIS_DAEMON_INVALID : BASIS_DAEMON_UNSUPPORTED;
}

static basis_daemon_status
unsupported(basis_daemon_event *event)
{
  set_event(event, BASIS_DAEMON_FAILED, 0, 0);
  return BASIS_DAEMON_UNSUPPORTED;
}

basis_daemon_status
basis_daemon_start(basis_daemon_supervisor *owner, const basis_daemon_runner *runners,
                   size_t runner_count, const basis_daemon_job *job, unsigned startup_ms,
                   unsigned reap_ms, basis_daemon_event *event)
{
  (void)owner; (void)runners; (void)runner_count; (void)job; (void)startup_ms; (void)reap_ms;
  return unsupported(event);
}

basis_daemon_status
basis_daemon_poll(basis_daemon_supervisor *owner, basis_daemon_event *event)
{
  (void)owner;
  return unsupported(event);
}

basis_daemon_status
basis_daemon_wait(basis_daemon_supervisor *owner, unsigned wait_ms, basis_daemon_event *event)
{
  (void)owner; (void)wait_ms;
  return unsupported(event);
}

basis_daemon_status
basis_daemon_stop(basis_daemon_supervisor *owner, unsigned grace_ms, unsigned reap_ms,
                  basis_daemon_event *event)
{
  (void)owner; (void)grace_ms; (void)reap_ms;
  return unsupported(event);
}

basis_daemon_status
basis_daemon_restart(basis_daemon_supervisor *owner, const basis_daemon_runner *runners,
                     size_t runner_count, const basis_daemon_job *job,
                     const basis_daemon_limits *limits, basis_daemon_event *event)
{
  (void)owner; (void)runners; (void)runner_count; (void)job; (void)limits;
  return unsupported(event);
}

basis_daemon_status
basis_daemon_run(basis_daemon_supervisor *owner, const basis_daemon_runner *runners,
                 size_t runner_count, const basis_daemon_job *job,
                 const basis_daemon_limits *limits, basis_daemon_event *event)
{
  (void)owner; (void)runners; (void)runner_count; (void)job; (void)limits;
  return unsupported(event);
}

basis_daemon_status
basis_daemon_queue_create(size_t capacity, basis_daemon_queue **out)
{
  (void)capacity; (void)out;
  return BASIS_DAEMON_UNSUPPORTED;
}

basis_daemon_status
basis_daemon_queue_destroy(basis_daemon_queue **out)
{
  (void)out;
  return BASIS_DAEMON_UNSUPPORTED;
}

basis_daemon_status
basis_daemon_queue_submit(basis_daemon_queue *q, const basis_daemon_runner *runners,
                          size_t runner_count, const basis_daemon_job *job,
                          const basis_daemon_policy *policy, uint64_t *id)
{
  (void)q; (void)runners; (void)runner_count; (void)job; (void)policy; (void)id;
  return BASIS_DAEMON_UNSUPPORTED;
}

basis_daemon_status
basis_daemon_queue_step(basis_daemon_queue *q, basis_daemon_queue_event *out)
{
  (void)q;
  queue_event_init(out);
  return BASIS_DAEMON_UNSUPPORTED;
}

basis_daemon_status
basis_daemon_queue_cancel(basis_daemon_queue *q, uint64_t id, basis_daemon_queue_event *out)
{
  (void)id;
  return basis_daemon_queue_step(q, out);
}

basis_daemon_status
basis_daemon_queue_shutdown(basis_daemon_queue *q, basis_daemon_queue_event *out)
{
  return basis_daemon_queue_step(q, out);
}

#endif
