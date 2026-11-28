#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "bench_host.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stddef.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

extern char **environ;

static double elapsed(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) * 1000000000.0
         + (double)(end.tv_nsec - start.tv_nsec);
}

static void stop_child(pid_t child) {
    int status;
    (void)kill(-child, SIGKILL);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
}

int on1x_bench_run(const char *const argv[], const char *stdout_path,
                   const char *stderr_path, unsigned timeout_ms,
                   on1x_bench_result *result) {
    posix_spawn_file_actions_t files;
    posix_spawnattr_t attributes;
    struct timespec start, now;
    pid_t child;
    int error, status, completion_fd = -1;
    size_t count;
    if (!argv || !argv[0] || !argv[0][0] || !stdout_path || !stderr_path
        || !result || !timeout_ms) { errno = EINVAL; return -1; }
    for (count = 0; count < 4097 && argv[count]; ++count) {}
    if (count > 4096) { errno = E2BIG; return -1; }
    *result = (on1x_bench_result){-1, 0, 0, 0};
    if ((error = posix_spawn_file_actions_init(&files))) goto failed;
    if ((error = posix_spawnattr_init(&attributes))) {
        posix_spawn_file_actions_destroy(&files);
        goto failed;
    }
    if ((error = posix_spawn_file_actions_addopen(&files, STDIN_FILENO,
                                                 "/dev/null", O_RDONLY, 0))
        || (error = posix_spawn_file_actions_addopen(&files, STDOUT_FILENO,
                              stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600))
        || (error = posix_spawn_file_actions_addopen(&files, STDERR_FILENO,
                              stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0600))
        || (error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP))
        || (error = posix_spawnattr_setpgroup(&attributes, 0))) goto destroy;
    if (clock_gettime(CLOCK_MONOTONIC, &start)) { error = errno; goto destroy; }
    error = posix_spawnp(&child, argv[0], &files, &attributes,
                        (char *const *)argv, environ);
destroy:
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&files);
    if (error) goto failed;
#if defined(__linux__) && defined(SYS_pidfd_open)
    /* Readiness follows actual exit, avoiding a polling floor for short jobs.
     * Older kernels retain the portable fallback below. No signal mask or
     * process-global SIGCHLD handler is changed by this measurement helper. */
    completion_fd = (int)syscall(SYS_pidfd_open, child, 0);
#endif
    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited < 0 && errno == EINTR) continue;
        if (waited < 0) { error = errno; stop_child(child); goto failed; }
        if (clock_gettime(CLOCK_MONOTONIC, &now)) {
            error = errno; stop_child(child); goto failed;
        }
        result->elapsed_ns = elapsed(start, now);
        if (waited == child) break;
        if (result->elapsed_ns >= (double)timeout_ms * 1000000.0) {
            stop_child(child);
            result->timed_out = 1;
            result->signal_number = SIGKILL;
            if (clock_gettime(CLOCK_MONOTONIC, &now)) { error = errno; goto failed; }
            result->elapsed_ns = elapsed(start, now);
            if (completion_fd >= 0) close(completion_fd);
            return 0;
        }
        int waiting;
        if (completion_fd >= 0) {
            double remaining = (double)timeout_ms - result->elapsed_ns / 1000000.0;
            int milliseconds = remaining >= INT_MAX ? INT_MAX : (int)remaining + 1;
            struct pollfd completion = {completion_fd, POLLIN, 0};
            waiting = poll(&completion, 1, milliseconds);
            if (waiting > 0 && (completion.revents & POLLNVAL)) {
                errno = EBADF; waiting = -1;
            }
        } else {
            /* POSIX fallback: whole-process wall time with <=1ms polling. */
            struct timespec pause = {0, 1000000};
            waiting = nanosleep(&pause, NULL);
        }
        if (waiting < 0 && errno != EINTR) {
            error = errno; stop_child(child); goto failed;
        }
    }
    if (completion_fd >= 0) close(completion_fd);
    if (WIFEXITED(status)) result->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) result->signal_number = WTERMSIG(status);
    return 0;
failed:
    if (completion_fd >= 0) close(completion_fd);
    errno = error;
    return -1;
}
