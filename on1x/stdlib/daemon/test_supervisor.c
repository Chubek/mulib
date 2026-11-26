#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "supervisor.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__linux__) || defined(MULIB_DAEMON_NO_POSIX) || defined(MULIB_PROCESS_NO_POSIX)

int main(void)
{
  basis_daemon_supervisor *owner = NULL;
  basis_daemon_queue *queue = NULL;
  basis_daemon_event event;
  basis_daemon_queue_event queued;
  assert(basis_daemon_create_supervisor(&owner) == BASIS_DAEMON_UNSUPPORTED);
  assert(!owner);
  assert(basis_daemon_start(NULL, NULL, 0, NULL, 200, 200, &event) == BASIS_DAEMON_UNSUPPORTED);
  assert(event.state == BASIS_DAEMON_FAILED && !event.child_owned);
  assert(basis_daemon_queue_create(1, &queue) == BASIS_DAEMON_UNSUPPORTED && !queue);
  assert(basis_daemon_queue_step(queue, &queued) == BASIS_DAEMON_UNSUPPORTED);
  assert(!queued.process.child_owned);
  return 0;
}

#else

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int stall_exec;
static int hold_reap;
static int fake_traced_stop;
static volatile sig_atomic_t interrupted;
static int allocations_until_failure = -1;

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
int __real_execve(const char *path, char *const argv[], char *const envp[]);
pid_t __real_waitpid(pid_t pid, int *status, int options);

static int fail_allocation(void)
{
  if (allocations_until_failure < 0) return 0;
  if (allocations_until_failure-- != 0) return 0;
  errno = ENOMEM;
  return 1;
}

void *__wrap_malloc(size_t size)
{
  return fail_allocation() ? NULL : __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size)
{
  return fail_allocation() ? NULL : __real_calloc(count, size);
}

int __wrap_execve(const char *path, char *const argv[], char *const envp[])
{
  if (stall_exec)
    for (;;) (void)pause();
  return __real_execve(path, argv, envp);
}

pid_t __wrap_waitpid(pid_t pid, int *status, int options)
{
  if (hold_reap && (options & WNOHANG)) return 0;
  if (fake_traced_stop && pid > 0 && (options & WNOHANG)) {
    fake_traced_stop = 0;
    *status = (SIGSTOP << 8) | 0x7f; /* Linux wait status for a ptrace stop. */
    assert(WIFSTOPPED(*status));
    return pid;
  }
  return __real_waitpid(pid, status, options);
}

static void on_term(int sig)
{
  (void)sig;
  _exit(23);
}

static void on_alarm(int sig)
{
  (void)sig;
  interrupted = 1;
}

static int runner(int argc, char **argv)
{
  unsigned char blob[BASIS_DAEMON_MAX_CONTEXT];
  const char *args[5];
  size_t length, i;
  struct stat st;
  int fd;
  char pid[32];
  int pid_size;
  if (argc > (int)(sizeof args / sizeof args[0])) return 100;
  for (i = 0; i < (size_t)argc; ++i) args[i] = argv[i];
  if (basis_daemon_decode_context(argc, args, "fixture", blob, sizeof blob, &length)) return 100;
  if (!length || getsid(0) != getpid() || getpgrp() != getpid()) return 101;
  if (getenv("PATH") || getenv("DAEMON_TEST_INHERITED")) return 102;
  for (fd = 0; fd < 3; ++fd)
    if (fstat(fd, &st) || !S_ISCHR(st.st_mode)) return 103;
  if (blob[0] == 0) {
    if (length != sizeof blob) return 104;
    for (i = 0; i < length; ++i)
      if (blob[i] != (unsigned char)i) return 105;
    return 37;
  }
  if (length != 1) return 106;
  if (blob[0] == 'S') return 0;
  if (blob[0] == 'X') {
    if (raise(SIGUSR1)) return 107;
    return 108;
  }
  if (blob[0] != 'T' && blob[0] != 'K') return 109;
  if (signal(SIGTERM, blob[0] == 'T' ? on_term : SIG_IGN) == SIG_ERR) return 110;
  fd = open("ready", O_CREAT | O_EXCL | O_WRONLY, 0666);
  if (fd < 0 || fstat(fd, &st) || (st.st_mode & 0777) != 0640) return 111;
  pid_size = snprintf(pid, sizeof pid, "%ld\n", (long)getpid());
  if (pid_size <= 0 || write(fd, pid, (size_t)pid_size) != pid_size || close(fd))
    return 112;
  for (;;) (void)pause();
}

static pid_t await_ready(const char *dir)
{
  char path[PATH_MAX];
  long pid = 0;
  int elapsed, rc = snprintf(path, sizeof path, "%s/ready", dir);
  assert(rc > 0 && (size_t)rc < sizeof path);
  for (elapsed = 0; elapsed < 2000 && !pid; elapsed += 5) {
    FILE *f = fopen(path, "r");
    if (f) {
      if (fscanf(f, "%ld", &pid) == 1) {
        assert(!fclose(f));
        assert(!unlink(path));
        break;
      }
      assert(!fclose(f));
    }
    (void)poll(NULL, 0, 5);
  }
  assert(pid > 0 && (long)(pid_t)pid == pid);
  return (pid_t)pid;
}

static void no_children(void)
{
  int status;
  errno = 0;
  assert(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
}

static void descriptor_and_limit_validation(void)
{
  basis_daemon_job job = {"a", NULL, 0};
  basis_daemon_runner runners[] = {{"a", "/a", "/"}, {"b", "/b", "/"}};
  basis_daemon_limits lim = {1, BASIS_DAEMON_MAX_MS, 0, 1};
  basis_daemon_policy policy = {lim, BASIS_DAEMON_RESTART_ALWAYS, BASIS_DAEMON_MAX_RESTARTS, 1};
  size_t length = 1234;
  unsigned char output[2] = {77, 77};
  const char *args[] = {"runner", "a", "000a"};
  assert(basis_daemon_validate_job(runners, 2, &job) == BASIS_DAEMON_OK);
  assert(basis_daemon_validate_job(runners, SIZE_MAX, &job) == BASIS_DAEMON_INVALID);
  job.context_len = SIZE_MAX;
  assert(basis_daemon_validate_job(runners, 2, &job) == BASIS_DAEMON_INVALID);
  job.context_len = 1;
  assert(basis_daemon_validate_job(runners, 2, &job) == BASIS_DAEMON_INVALID);
  job.context_len = 0;
  job.runner = "b../oops";
  assert(basis_daemon_validate_job(runners, 2, &job) == BASIS_DAEMON_INVALID);
  job.runner = "not-approved";
  assert(basis_daemon_validate_job(runners, 2, &job) == BASIS_DAEMON_INVALID);
  job.runner = "a";
  runners[1].executable = "relative";
  assert(basis_daemon_validate_job(runners, 2, &job) == BASIS_DAEMON_INVALID);
  runners[1].executable = "/bad\npath";
  assert(basis_daemon_validate_job(runners, 2, &job) == BASIS_DAEMON_INVALID);
  runners[1].executable = "/b";
  runners[1].name = "a";
  assert(basis_daemon_validate_job(runners, 2, &job) == BASIS_DAEMON_INVALID);
  assert(basis_daemon_validate_limits(&lim) == BASIS_DAEMON_OK);
  assert(basis_daemon_validate_policy(&policy) == BASIS_DAEMON_OK);
  policy.max_restarts = UINT_MAX;
  assert(basis_daemon_validate_policy(&policy) == BASIS_DAEMON_INVALID);
  lim.startup_ms = 0;
  assert(basis_daemon_validate_limits(&lim) == BASIS_DAEMON_INVALID);
  lim.startup_ms = 1;
  lim.reap_ms = UINT_MAX;
  assert(basis_daemon_validate_limits(&lim) == BASIS_DAEMON_INVALID);
  lim.reap_ms = 1;
  lim.grace_ms = UINT_MAX;
  assert(basis_daemon_validate_limits(&lim) == BASIS_DAEMON_INVALID);
  assert(basis_daemon_decode_context(3, args, "b", output, sizeof output, &length)
         == BASIS_DAEMON_INVALID);
  assert(basis_daemon_decode_context(3, args, "a", output, 1, &length)
         == BASIS_DAEMON_INVALID);
  assert(length == 1234 && output[0] == 77 && output[1] == 77);
  assert(basis_daemon_decode_context(3, args, "a", output, sizeof output, &length)
         == BASIS_DAEMON_OK);
  assert(length == 2 && output[0] == 0 && output[1] == 10);
  args[2] = "0A";
  assert(basis_daemon_decode_context(3, args, "a", output, sizeof output, &length)
         == BASIS_DAEMON_INVALID);
  args[2] = "a";
  assert(basis_daemon_decode_context(3, args, "a", output, sizeof output, &length)
         == BASIS_DAEMON_INVALID);
  args[2] = "";
  assert(basis_daemon_decode_context(3, args, "a", NULL, 0, &length) == BASIS_DAEMON_OK);
  assert(!length);
}

static void allocation_and_descriptor_failures(basis_daemon_supervisor **owner,
                                               const basis_daemon_job *job,
                                               const basis_daemon_runner *entry)
{
  basis_daemon_event event;
  struct rlimit before, lowered;
  int fd[128], count = 0, i, first_success = 0;
  allocations_until_failure = 0;
  assert(basis_daemon_create_supervisor(owner) == BASIS_DAEMON_NOMEM);
  assert(!*owner);
  assert(basis_daemon_create_supervisor(owner) == BASIS_DAEMON_OK && *owner);
  assert(basis_daemon_create_supervisor(owner) == BASIS_DAEMON_INVALID);
  for (i = 0; i < 32; ++i) {
    basis_daemon_status rc;
    allocations_until_failure = i;
    rc = basis_daemon_start(*owner, entry, 1, job, 2000, 2000, &event);
    allocations_until_failure = -1;
    if (!rc) {
      assert(i > 0);
      if (event.child_owned)
        assert(basis_daemon_wait(*owner, 2000, &event) == BASIS_DAEMON_OK);
      assert(event.state == BASIS_DAEMON_EXITED && event.exit_code == 37 && !event.child_owned);
      first_success = 1;
      break;
    }
    assert(rc == BASIS_DAEMON_NOMEM && !event.child_owned);
    no_children();
  }
  assert(first_success);
  no_children();

  assert(!getrlimit(RLIMIT_NOFILE, &before));
  lowered = before;
  if (lowered.rlim_cur > 32) lowered.rlim_cur = 32;
  assert(!setrlimit(RLIMIT_NOFILE, &lowered));
  while (count < (int)(sizeof fd / sizeof fd[0])
         && (fd[count] = open("/dev/null", O_RDONLY | O_CLOEXEC)) >= 0)
    ++count;
  assert(errno == EMFILE && count < (int)(sizeof fd / sizeof fd[0]));
  assert(basis_daemon_start(*owner, entry, 1, job, 1000, 1000, &event) == BASIS_DAEMON_SYSTEM);
  assert(event.state == BASIS_DAEMON_START_FAILED && event.system_error == EMFILE && !event.child_owned);
  while (count) assert(!close(fd[--count]));
  assert(!setrlimit(RLIMIT_NOFILE, &before));
  no_children();
}

static basis_daemon_queue_event drive_to_terminal(basis_daemon_queue *queue, uint64_t id,
                                                   unsigned restarts)
{
  basis_daemon_queue_event event;
  unsigned i, seen = 0;
  for (i = 0; i < 2000; ++i) {
    assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK);
    assert(event.id == id);
    if (event.state == BASIS_DAEMON_QUEUE_BACKOFF && seen < event.attempt)
      seen = event.attempt;
    if (event.state == BASIS_DAEMON_QUEUE_FINISHED) {
      assert(seen == restarts && event.attempt == restarts + 1);
      assert(!event.process.child_owned);
      return event;
    }
    assert(event.state == BASIS_DAEMON_QUEUE_RUNNING || event.state == BASIS_DAEMON_QUEUE_BACKOFF);
    (void)poll(NULL, 0, 1);
  }
  assert(!"queue did not finish");
  return event;
}

static void background_queue(const basis_daemon_runner *entry, const char *scratch)
{
  basis_daemon_queue *queue = NULL;
  basis_daemon_queue_event event;
  basis_daemon_policy once = {{2000, 2000, 1000, 2000}, BASIS_DAEMON_RESTART_NEVER, 0, 0};
  basis_daemon_policy retry = {{2000, 2000, 1000, 2000}, BASIS_DAEMON_RESTART_ALWAYS, 1, 10};
  unsigned char type = 'T';
  basis_daemon_job job = {"fixture", &type, 1};
  basis_daemon_runner missing = *entry;
  uint64_t first, second, third, fourth = UINT64_MAX;
  unsigned attempts;

  assert(basis_daemon_queue_create(0, &queue) == BASIS_DAEMON_INVALID && !queue);
  assert(basis_daemon_queue_create(SIZE_MAX, &queue) == BASIS_DAEMON_INVALID && !queue);
  allocations_until_failure = 0;
  assert(basis_daemon_queue_create(2, &queue) == BASIS_DAEMON_NOMEM && !queue);
  assert(basis_daemon_queue_create(2, &queue) == BASIS_DAEMON_OK);
  allocations_until_failure = 0;
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &retry, &fourth) == BASIS_DAEMON_NOMEM);
  assert(fourth == UINT64_MAX);
  assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK && event.state == BASIS_DAEMON_QUEUE_IDLE);
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &retry, &first) == BASIS_DAEMON_OK);
  type = 'X';
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &once, &second) == BASIS_DAEMON_OK);
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &once, &fourth) == BASIS_DAEMON_LIMIT);
  assert(fourth == UINT64_MAX && first < second);
  assert(basis_daemon_queue_destroy(&queue) == BASIS_DAEMON_BUSY && queue);
  assert(basis_daemon_queue_cancel(queue, second, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_QUEUE_CANCELLED && event.id == second && !event.attempt);
  assert(basis_daemon_queue_cancel(queue, second, &event) == BASIS_DAEMON_NOT_FOUND);
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &once, &third) == BASIS_DAEMON_OK);
  assert(second < third);
  type = 'K'; /* Submitted context was copied. */
  assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK);
  assert(event.id == first && event.state == BASIS_DAEMON_QUEUE_RUNNING && event.pending == 1);
  (void)await_ready(scratch);
  assert(basis_daemon_queue_cancel(queue, first, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_QUEUE_CANCELLED && event.process.exit_code == 23);
  event = drive_to_terminal(queue, third, 0);
  assert(event.process.state == BASIS_DAEMON_SIGNALED && event.process.term_signal == SIGUSR1);
  no_children();

  type = 'S';
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &retry, &first) == BASIS_DAEMON_OK);
  event = drive_to_terminal(queue, first, 1);
  assert(event.process.state == BASIS_DAEMON_EXITED && !event.process.exit_code);
  retry.restart = BASIS_DAEMON_RESTART_ON_FAILURE;
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &retry, &first) == BASIS_DAEMON_OK);
  event = drive_to_terminal(queue, first, 0);
  assert(event.process.state == BASIS_DAEMON_EXITED && !event.process.exit_code);
  missing.executable = "/definitely/missing/on1x-daemon-queue";
  assert(basis_daemon_queue_submit(queue, &missing, 1, &job, &retry, &first) == BASIS_DAEMON_OK);
  event = drive_to_terminal(queue, first, 1);
  assert(event.process_status == BASIS_DAEMON_SYSTEM && event.process.state == BASIS_DAEMON_START_FAILED
         && event.process.system_error == ENOENT);
  no_children();

  type = 'K';
  once.limits.grace_ms = 0;
  once.limits.run_ms = 50;
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &once, &first) == BASIS_DAEMON_OK);
  assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK
         && event.state == BASIS_DAEMON_QUEUE_RUNNING);
  (void)await_ready(scratch);
  event = drive_to_terminal(queue, first, 0);
  assert(event.process_status == BASIS_DAEMON_TIMEOUT && event.process.state == BASIS_DAEMON_RUN_TIMEOUT
         && event.process.term_signal == SIGKILL);
  no_children();

  retry.limits.grace_ms = 0;
  retry.limits.reap_ms = 10;
  retry.limits.run_ms = 30;
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &retry, &first) == BASIS_DAEMON_OK);
  assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK);
  (void)await_ready(scratch);
  (void)poll(NULL, 0, 40);
  hold_reap = 1;
  assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_QUEUE_CLEANUP && event.attempt == 1
         && event.process.state == BASIS_DAEMON_RUN_TIMEOUT && event.process.child_owned);
  assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_QUEUE_CLEANUP && event.attempt == 1 && event.process.child_owned);
  hold_reap = 0;
  for (attempts = 0; attempts < 100; ++attempts) {
    assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK);
    if (event.state != BASIS_DAEMON_QUEUE_CLEANUP) break;
  }
  assert(event.state == BASIS_DAEMON_QUEUE_BACKOFF && event.attempt == 1 && !event.process.child_owned);
  assert(basis_daemon_queue_cancel(queue, first, &event) == BASIS_DAEMON_OK
         && event.state == BASIS_DAEMON_QUEUE_CANCELLED);
  no_children();

  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &retry, &first) == BASIS_DAEMON_OK);
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &retry, &second) == BASIS_DAEMON_OK);
  assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK);
  (void)await_ready(scratch);
  hold_reap = 1;
  assert(basis_daemon_queue_shutdown(queue, &event) == BASIS_DAEMON_TIMEOUT);
  assert(event.state == BASIS_DAEMON_QUEUE_CLEANUP && event.process.child_owned && event.dropped == 1);
  assert(basis_daemon_queue_destroy(&queue) == BASIS_DAEMON_BUSY);
  assert(basis_daemon_queue_submit(queue, entry, 1, &job, &retry, &fourth) == BASIS_DAEMON_CLOSED);
  hold_reap = 0;
  for (attempts = 0; attempts < 100; ++attempts) {
    assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK);
    if (event.state != BASIS_DAEMON_QUEUE_CLEANUP) break;
  }
  assert(event.state == BASIS_DAEMON_QUEUE_CANCELLED && event.id == first);
  assert(basis_daemon_queue_step(queue, &event) == BASIS_DAEMON_OK && event.state == BASIS_DAEMON_QUEUE_IDLE);
  assert(basis_daemon_queue_destroy(&queue) == BASIS_DAEMON_OK && !queue);
  no_children();
}

int main(int argc, char **argv)
{
  char *scratch;
  char executable[PATH_MAX];
  unsigned char blob[BASIS_DAEMON_MAX_CONTEXT], type;
  basis_daemon_supervisor *owner = NULL;
  basis_daemon_runner entry, broken;
  basis_daemon_job job = {"fixture", blob, sizeof blob};
  basis_daemon_event event;
  basis_daemon_limits limits = {2000, 2000, 100, 2000};
  ssize_t len;
  size_t i;
  int status;
  pid_t pid;
  sigset_t mask;
  struct sigaction act, saved;
  struct itimerval alarm = {{0, 1000}, {0, 1000}};
  const struct itimerval no_alarm = {{0, 0}, {0, 0}};
  if (argc > 2) return runner(argc, argv);
  assert(argc == 2 && argv[1][0] == '/');
  assert(!sigemptyset(&mask) && !sigprocmask(SIG_SETMASK, &mask, NULL));
  scratch = argv[1];
  len = readlink("/proc/self/exe", executable, sizeof executable - 1);
  assert(len > 0 && (size_t)len < sizeof executable - 1);
  executable[len] = '\0';
  entry = (basis_daemon_runner){"fixture", executable, scratch};
  descriptor_and_limit_validation();
  for (i = 0; i < sizeof blob; ++i) blob[i] = (unsigned char)i;
  allocation_and_descriptor_failures(&owner, &job, &entry);
  assert(basis_daemon_poll(owner, &event) == BASIS_DAEMON_OK
         && event.state == BASIS_DAEMON_START_FAILED && event.system_error == EMFILE);
  assert(!setenv("DAEMON_TEST_INHERITED", "must not pass", 1));
  assert(basis_daemon_run(owner, &entry, 1, &job, &limits, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_EXITED && event.exit_code == 37 && !event.child_owned);
  assert(basis_daemon_stop(owner, 0, 1000, &event) == BASIS_DAEMON_OK && event.state == BASIS_DAEMON_IDLE);
  assert(basis_daemon_poll(owner, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_EXITED && event.exit_code == 37 && !event.child_owned);
  no_children();

  job.context = &type;
  job.context_len = 1;
  type = 'X';
  assert(basis_daemon_run(owner, &entry, 1, &job, &limits, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_SIGNALED && event.term_signal == SIGUSR1 && !event.child_owned);
  no_children();

  broken = entry;
  broken.executable = "/definitely/missing/on1x-daemon-fixture";
  assert(basis_daemon_start(owner, &broken, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_SYSTEM);
  assert(event.state == BASIS_DAEMON_START_FAILED && event.system_error == ENOENT && !event.child_owned);
  broken = entry;
  broken.directory = "/definitely/missing/on1x-daemon-cwd";
  assert(basis_daemon_start(owner, &broken, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_SYSTEM);
  assert(event.state == BASIS_DAEMON_START_FAILED && event.system_error == ENOENT && !event.child_owned);
  no_children();

  memset(&act, 0, sizeof act);
  act.sa_handler = SIG_IGN;
  assert(!sigemptyset(&act.sa_mask));
  assert(!sigaction(SIGCHLD, &act, &saved));
  assert(basis_daemon_start(owner, &entry, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_OWNERSHIP);
  assert(event.state == BASIS_DAEMON_START_FAILED && !event.child_owned);
  assert(!sigaction(SIGCHLD, &saved, NULL));
  no_children();

  act.sa_handler = on_alarm;
  assert(!sigaction(SIGALRM, &act, &saved));
  assert(!setitimer(ITIMER_REAL, &alarm, NULL));
  stall_exec = 1;
  assert(basis_daemon_start(owner, &entry, 1, &job, 20, 1000, &event) == BASIS_DAEMON_TIMEOUT);
  stall_exec = 0;
  assert(!setitimer(ITIMER_REAL, &no_alarm, NULL));
  assert(interrupted);
  assert(event.state == BASIS_DAEMON_START_TIMEOUT && !event.child_owned && event.term_signal == SIGKILL);
  no_children();

  type = 'T';
  assert(basis_daemon_start(owner, &entry, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_RUNNING && event.child_owned);
  (void)await_ready(scratch);
  fake_traced_stop = 1;
  assert(basis_daemon_poll(owner, &event) == BASIS_DAEMON_OK
         && event.child_owned && event.state == BASIS_DAEMON_RUNNING);
  assert(basis_daemon_start(owner, &entry, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_BUSY);
  assert(event.child_owned);
  assert(basis_daemon_destroy_supervisor(&owner) == BASIS_DAEMON_BUSY && owner);
  assert(basis_daemon_wait(owner, 5, &event) == BASIS_DAEMON_OK && event.state == BASIS_DAEMON_RUNNING);
  pid = fork();
  assert(pid >= 0);
  if (!pid) {
    if (basis_daemon_poll(owner, &event) != BASIS_DAEMON_OWNERSHIP
        || basis_daemon_destroy_supervisor(&owner) != BASIS_DAEMON_OWNERSHIP) _exit(120);
    _exit(0);
  }
  assert(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && !WEXITSTATUS(status));
  assert(basis_daemon_stop(owner, 1000, 1000, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_STOPPED && event.exit_code == 23 && !event.child_owned);
  no_children();

  type = 'K';
  assert(basis_daemon_start(owner, &entry, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_OK);
  (void)await_ready(scratch);
  assert(basis_daemon_stop(owner, 25, 1000, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_STOPPED && event.term_signal == SIGKILL && !event.child_owned);
  no_children();

  assert(basis_daemon_start(owner, &entry, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_OK);
  (void)await_ready(scratch);
  hold_reap = 1;
  assert(basis_daemon_stop(owner, 0, 10, &event) == BASIS_DAEMON_TIMEOUT);
  assert(event.state == BASIS_DAEMON_STOP_TIMEOUT && event.child_owned);
  assert(basis_daemon_destroy_supervisor(&owner) == BASIS_DAEMON_BUSY);
  hold_reap = 0;
  assert(basis_daemon_wait(owner, 1000, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_SIGNALED && event.term_signal == SIGKILL && !event.child_owned);
  no_children();

  assert(basis_daemon_start(owner, &entry, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_OK);
  (void)await_ready(scratch);
  job.runner = "not-in-table";
  assert(basis_daemon_restart(owner, &entry, 1, &job, &limits, &event) == BASIS_DAEMON_INVALID);
  assert(event.child_owned && basis_daemon_poll(owner, &event) == BASIS_DAEMON_OK
         && event.state == BASIS_DAEMON_RUNNING);
  job.runner = "fixture";
  type = 'X';
  assert(basis_daemon_restart(owner, &entry, 1, &job, &limits, &event) == BASIS_DAEMON_OK);
  if (event.state == BASIS_DAEMON_RUNNING)
    assert(basis_daemon_wait(owner, 2000, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_SIGNALED && event.term_signal == SIGUSR1 && !event.child_owned);
  no_children();

  type = 'K';
  assert(basis_daemon_start(owner, &entry, 1, &job, 2000, 1000, &event) == BASIS_DAEMON_OK);
  pid = await_ready(scratch);
  /* Deliberately violate the sole-reaper contract. No stale pid is retained. */
  assert(!kill(pid, SIGKILL));
  assert(waitpid(pid, &status, 0) == pid && WIFSIGNALED(status));
  assert(basis_daemon_poll(owner, &event) == BASIS_DAEMON_OWNERSHIP);
  assert(event.system_error == ECHILD && !event.child_owned);
  no_children();

  limits.run_ms = 100;
  limits.grace_ms = 0;
  assert(basis_daemon_run(owner, &entry, 1, &job, &limits, &event) == BASIS_DAEMON_TIMEOUT);
  assert(event.state == BASIS_DAEMON_RUN_TIMEOUT && event.term_signal == SIGKILL && !event.child_owned);
  /* The worker may have been scheduled before the deadline and left its marker. */
  assert(!chdir(scratch));
  assert(!unlink("ready") || errno == ENOENT);
  no_children();
  assert(basis_daemon_destroy_supervisor(&owner) == BASIS_DAEMON_OK && !owner);
  background_queue(&entry, scratch);
  puts("daemon supervision: ok");
  return 0;
}

#endif
