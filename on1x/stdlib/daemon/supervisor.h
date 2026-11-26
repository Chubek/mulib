#ifndef BASIS_DAEMON_SUPERVISOR_H
#define BASIS_DAEMON_SUPERVISOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Explicit native host API, not an On1x FFI. No raw process ID is exported.
 * All calls for an owner are serialized, in the creating process. No other
 * code may reap these children. See README.md for the launch trust boundary. */
typedef struct basis_daemon_supervisor basis_daemon_supervisor;
typedef struct basis_daemon_queue basis_daemon_queue;

enum {
  BASIS_DAEMON_MAX_RUNNERS = 64,
  BASIS_DAEMON_MAX_CONTEXT = 4096,
  BASIS_DAEMON_MAX_MS = 60000,
  BASIS_DAEMON_MAX_PENDING = 16,
  BASIS_DAEMON_MAX_RESTARTS = 8
};

typedef struct {
  const char *name;
  const char *executable; /* Absolute trusted executable path, never PATH. */
  const char *directory;  /* Absolute trusted working directory. */
} basis_daemon_runner;

typedef struct {
  const char *runner;
  const unsigned char *context; /* NULL allowed only for length zero. */
  size_t context_len;
} basis_daemon_job;

typedef struct {
  unsigned startup_ms, run_ms, grace_ms, reap_ms;
} basis_daemon_limits;

typedef enum {
  BASIS_DAEMON_OK = 0,
  BASIS_DAEMON_INVALID = -1,
  BASIS_DAEMON_NOMEM = -2,
  BASIS_DAEMON_SYSTEM = -3,
  BASIS_DAEMON_UNSUPPORTED = -4,
  BASIS_DAEMON_BUSY = -5,
  BASIS_DAEMON_TIMEOUT = -6,
  BASIS_DAEMON_OWNERSHIP = -7,
  BASIS_DAEMON_NOT_FOUND = -8,
  BASIS_DAEMON_LIMIT = -9,
  BASIS_DAEMON_CLOSED = -10
} basis_daemon_status;

typedef enum {
  BASIS_DAEMON_IDLE = 0,
  BASIS_DAEMON_RUNNING,
  BASIS_DAEMON_EXITED,
  BASIS_DAEMON_SIGNALED,
  BASIS_DAEMON_STOPPED,
  BASIS_DAEMON_START_FAILED,
  BASIS_DAEMON_START_TIMEOUT,
  BASIS_DAEMON_RUN_TIMEOUT,
  BASIS_DAEMON_STOP_TIMEOUT,
  BASIS_DAEMON_FAILED
} basis_daemon_state;

typedef struct {
  basis_daemon_state state;
  int exit_code;    /* -1 unless reaped with a normal exit code. */
  int term_signal;  /* 0 unless reaped due to a signal. */
  int system_error; /* POSIX errno, or 0 if not applicable. */
  int child_owned;  /* A child still requires supervision/reaping. */
} basis_daemon_event;

/* Start with a NULL owner slot. Destroy clears the slot; never drops a child. */
basis_daemon_status basis_daemon_create_supervisor(basis_daemon_supervisor **owner);
basis_daemon_status basis_daemon_destroy_supervisor(basis_daemon_supervisor **owner);

/* Checks the complete allowlist (including duplicate names), job name, bound,
 * and authorization without starting or opening anything. Callers validate
 * application-specific context schema separately. */
basis_daemon_status basis_daemon_validate_job(const basis_daemon_runner *runners,
                                              size_t runner_count,
                                              const basis_daemon_job *job);
basis_daemon_status basis_daemon_validate_limits(const basis_daemon_limits *limits);

/* The approved executable receives argv[0] = executable, argv[1] = runner
 * name, argv[2..] = canonical lowercase hex chunks (2,047 raw bytes per full
 * chunk; at least one chunk even for empty context). This decoder validates the
 * entire envelope and expected runner before writing output. The output buffer
 * and length slot must not overlap each other or argv. */
basis_daemon_status basis_daemon_decode_context(int argc, const char *const *argv,
                                                 const char *expected_runner,
                                                 unsigned char *output,
                                                 size_t capacity, size_t *length);

/* All positive deadlines are <= BASIS_DAEMON_MAX_MS; grace may be zero.
 * Inputs are borrowed only during calls. A live child is never replaced by
 * start or run; use stop/restart. Every lifecycle call writes a non-NULL event,
 * including invalid and failed calls. On error, check event.child_owned. */
basis_daemon_status basis_daemon_start(basis_daemon_supervisor *owner,
                                       const basis_daemon_runner *runners, size_t runner_count,
                                       const basis_daemon_job *job, unsigned startup_ms,
                                       unsigned reap_ms, basis_daemon_event *event);
/* Observe current running state or reap and report its terminal state. With no
 * process, repeating poll/wait returns the last terminal event (initially IDLE).
 * A wait deadline is not termination: it returns OK with RUNNING. */
basis_daemon_status basis_daemon_poll(basis_daemon_supervisor *owner, basis_daemon_event *event);
basis_daemon_status basis_daemon_wait(basis_daemon_supervisor *owner, unsigned wait_ms,
                                      basis_daemon_event *event);

/* Graceful-stop via SIGTERM, then SIGKILL and bounded reap if necessary.
 * Zero grace skips SIGTERM. Stopping an idle owner is an IDLE no-op (the saved
 * event stays available via poll). A timeout/error never discards a child. */
basis_daemon_status basis_daemon_stop(basis_daemon_supervisor *owner, unsigned grace_ms,
                                      unsigned reap_ms, basis_daemon_event *event);

/* Validates and builds the next command before stopping the old child.
 * Launches the replacement only after its predecessor is fully reaped. */
basis_daemon_status basis_daemon_restart(basis_daemon_supervisor *owner,
                                         const basis_daemon_runner *runners, size_t runner_count,
                                         const basis_daemon_job *job, const basis_daemon_limits *limits,
                                         basis_daemon_event *event);

/* One bounded run. On run deadline, stops/reaps using the separate cleanup
 * limits; successful cleanup returns TIMEOUT with state RUN_TIMEOUT. */
basis_daemon_status basis_daemon_run(basis_daemon_supervisor *owner,
                                     const basis_daemon_runner *runners, size_t runner_count,
                                     const basis_daemon_job *job, const basis_daemon_limits *limits,
                                     basis_daemon_event *event);

typedef enum {
  BASIS_DAEMON_RESTART_NEVER = 0,
  BASIS_DAEMON_RESTART_ON_FAILURE,
  BASIS_DAEMON_RESTART_ALWAYS
} basis_daemon_restart_mode;

typedef struct {
  basis_daemon_limits limits;
  basis_daemon_restart_mode restart;
  unsigned max_restarts, backoff_ms;
} basis_daemon_policy;

typedef enum {
  BASIS_DAEMON_QUEUE_IDLE = 0,
  BASIS_DAEMON_QUEUE_RUNNING,
  BASIS_DAEMON_QUEUE_BACKOFF,
  BASIS_DAEMON_QUEUE_CLEANUP,
  BASIS_DAEMON_QUEUE_FINISHED,
  BASIS_DAEMON_QUEUE_CANCELLED
} basis_daemon_queue_state;

typedef struct {
  basis_daemon_queue_state state;
  uint64_t id; /* 0 for no job. IDs belong to this queue and never wrap. */
  unsigned attempt; /* One-based; 0 if a queued job never started. */
  size_t pending, dropped; /* `dropped` counts waiting jobs removed by shutdown. */
  basis_daemon_status process_status;
  basis_daemon_event process;
} basis_daemon_queue_event;

/* A host-driven, in-memory FIFO. The capacity counts waiting jobs; one other
 * child may be active. A restarted job keeps its active slot during backoff.
 * Enqueue copies the prepared command and policy. No host thread/timer or
 * callback is created. The host calls step to start, observe and advance jobs. */
basis_daemon_status basis_daemon_validate_policy(const basis_daemon_policy *policy);
basis_daemon_status basis_daemon_queue_create(size_t capacity, basis_daemon_queue **queue);
basis_daemon_status basis_daemon_queue_destroy(basis_daemon_queue **queue);
basis_daemon_status basis_daemon_queue_submit(basis_daemon_queue *queue,
                                              const basis_daemon_runner *runners, size_t runner_count,
                                              const basis_daemon_job *job, const basis_daemon_policy *policy,
                                              uint64_t *id);
/* One step handles at most one attempt. Inspect process_status and process
 * for attempt results; OK means the step was observed, not the job succeeded.
 * An unreaped child retains the active slot until cleanup succeeds. */
basis_daemon_status basis_daemon_queue_step(basis_daemon_queue *queue,
                                            basis_daemon_queue_event *event);
/* Cancellation of a waiting job never starts it. Cancelling an active job
 * suppresses restart even if cleanup fails or must be retried. */
basis_daemon_status basis_daemon_queue_cancel(basis_daemon_queue *queue, uint64_t id,
                                              basis_daemon_queue_event *event);
/* Close admission, remove waiting jobs and cancel active work. A later
 * shutdown/step may be needed on timeout; destroy refuses any remaining job. */
basis_daemon_status basis_daemon_queue_shutdown(basis_daemon_queue *queue,
                                                basis_daemon_queue_event *event);

#ifdef __cplusplus
}
#endif
#endif
