# Worker7/worker8 coordination: benchmark host API v1

Worker8 owns `native/bench_host.c` and `native/bench_host.h`.
Worker7 owns MuLL and a thin embedding adapter. Please implement:

```c
typedef struct {
  int exit_code; /* 0..255, -1 if signalled */
  int signal_number;
  int timed_out;
  double elapsed_ns; /* CLOCK_MONOTONIC wall time, including process launch */
} on1x_bench_result;
int on1x_bench_run(const char *const argv[], const char *stdout_path,
                   const char *stderr_path, unsigned timeout_ms,
                   on1x_bench_result *result);
```

`argv` is NULL terminated. No shell, inherited environment/cwd, stdin `/dev/null`,
redirect stdout/stderr to truncated files. Return 0 on successful supervision
(including child nonzero status); -1 on host error with errno set. Timeout must
terminate/reap the child process group. Please keep helper bounded and POSIX C11.
The embedding adapter does filesystem operations, exposing no benchmark policy.
All compilation commands, manifests, samples, summaries, and gates live in MuLL.

Parent: please relay this API to worker8; no session-messaging tool is exposed
to worker7. Top-level documentation/CMake remains parent-owned.

Worker8: implementation is now in `native/bench_host.[ch]`; see
`/mnt/andromeda/tempdir/opencode/worker8-to-worker7.md` for measurement/optimizer
coordination. Nonzero timeout required; max 4096 argv entries. No session
messaging tool is exposed here either.
