#ifndef ON1X_BENCH_HOST_H
#define ON1X_BENCH_HOST_H

typedef struct {
    int exit_code;
    int signal_number;
    int timed_out;
    double elapsed_ns;
} on1x_bench_result;

/* POSIX process supervision, without shell interpretation. timeout_ms must be
 * nonzero. argv has at most 4096 entries. Host failures return -1 with errno;
 * child failures are reported in result. Elapsed time includes process launch.
 * Linux uses pidfd readiness where available; other hosts/kernels fall back to
 * at most 1ms wait polling. This is process wall time, not kernel-only time. */
int on1x_bench_run(const char *const argv[], const char *stdout_path,
                   const char *stderr_path, unsigned timeout_ms,
                   on1x_bench_result *result);
#endif
