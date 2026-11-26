#ifndef ON1X_STDLIB_KRON_RUNTIME_H
#define ON1X_STDLIB_KRON_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BASIS_KRON_MAX_JOB_BYTES 65536u

typedef struct basis_kron_vm basis_kron_vm;
typedef enum { BASIS_KRON_ONCE, BASIS_KRON_EVERY } basis_kron_cadence;
typedef enum {
  BASIS_KRON_PERSISTENT, BASIS_KRON_FOR, BASIS_KRON_UNTIL_SIGNAL
} basis_kron_lifetime;
typedef struct {
  basis_kron_cadence cadence;
  int64_t seconds; /* Nonnegative absolute Unix deadline or positive interval. */
  basis_kron_lifetime lifetime;
  int64_t limit; /* Zero, positive seconds, or a signal in 1..INT32_MAX. */
} basis_kron_schedule;

typedef enum {
  BASIS_KRON_ERROR = -1, BASIS_KRON_COMMITTED = 0, BASIS_KRON_UNCERTAIN = 1
} basis_kron_change;
typedef enum {
  BASIS_KRON_NEXT_ERROR = -1, BASIS_KRON_EMPTY = 0, BASIS_KRON_READY = 1
} basis_kron_next_status;
typedef enum {
  BASIS_KRON_DISPATCH_ERROR = -1, BASIS_KRON_IDLE = 0, BASIS_KRON_CONSUMED = 1,
  BASIS_KRON_CONSUMED_UNCERTAIN = 2
} basis_kron_dispatch_status;
typedef enum {
  BASIS_KRON_EXPIRED, BASIS_KRON_DENIED, BASIS_KRON_RAN, BASIS_KRON_RUN_FAILED,
  BASIS_KRON_NOT_RUN_UNCERTAIN
} basis_kron_disposition;
typedef struct {
  uint64_t id;
  int64_t deadline;
  const unsigned char *data;
  size_t size;
} basis_kron_occurrence;
typedef struct {
  void *user;
  /* Both callbacks are required. Authorize parses/validates the entire
   * descriptor and checks current host policy; exactly 1 allows execution.
   * Returns from run are application-defined (0 success). Neither callback
   * may reenter the VM; the occurrence and its bytes are borrowed. */
  int (*authorize)(void *, const basis_kron_occurrence *);
  int (*run)(void *, const basis_kron_occurrence *);
} basis_kron_host;
typedef struct {
  uint64_t id;
  int64_t deadline;
  basis_kron_disposition disposition;
  int run_status; /* Meaningful only for RAN and RUN_FAILED. */
} basis_kron_result;

/* Use an explicit, trusted, existing absolute directory provisioned as 0700.
 * Both it and any existing regular snapshot must be owned by the effective
 * user with no group/other permissions; neither may be a final symlink.
 * Keep the directory, its parents, and their names stable. Both successful
 * statuses transfer a live VM; failure leaves out untouched. Do not overwrite
 * a slot that still owns a live VM. */
basis_kron_change basis_kron_open(const char *directory, basis_kron_vm **out);
/* Release the exclusive cache lock; does not cancel pending jobs. NULL is a
 * no-op; a callback's reentrant close is rejected with EBUSY. */
void basis_kron_close(basis_kron_vm *vm);
/* Pure preflight: 0 valid, -1 with errno for invalid values or overflow. */
int basis_kron_validate_schedule(const basis_kron_schedule *schedule, int64_t now);
/* Copies 1..BASIS_KRON_MAX_JOB_BYTES bytes. Clock supplied by the caller.
 * No code, credentials, policy, or executable pointers in descriptor data. */
basis_kron_change basis_kron_delegate(basis_kron_vm *vm,
                                     const basis_kron_schedule *schedule,
                                     int64_t now, const void *data,
                                     size_t size, uint64_t *id);
basis_kron_change basis_kron_pull(basis_kron_vm *vm, uint64_t id);
basis_kron_change basis_kron_signal(basis_kron_vm *vm, int signal);
basis_kron_next_status basis_kron_next(const basis_kron_vm *vm,
                                     int64_t *deadline);
/* Consume at most one occurrence. On idle/error, out is untouched and no
 * callback runs. Negative returns set errno. Uncertain consumption suppresses
 * callbacks but still updates this live VM; don't retry as if it failed.
 * Serialize operations on each VM; no background clock or dispatch thread. */
basis_kron_dispatch_status basis_kron_dispatch(basis_kron_vm *vm, int64_t now,
                                             const basis_kron_host *host,
                                             basis_kron_result *out);

#ifdef __cplusplus
}
#endif
#endif
