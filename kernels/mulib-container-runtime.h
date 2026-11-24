/** @file mulib-container-runtime.h
 * @brief Backend-driven lifecycle execution with bounded polling and rollback.
 * Define MULIB_IMPL_CONTAINER_RUNTIME plus CONTAINER and ROOTFS dependencies. */
#ifndef MULIB_CONTAINER_RUNTIME_H
#define MULIB_CONTAINER_RUNTIME_H
#include "mulib-container.h"
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C" {
#endif
enum
{
  MULIB_CONTAINER_CAP_PAUSE = 1, MULIB_CONTAINER_CAP_SIGNAL = 2,
  MULIB_CONTAINER_CAP_ROOTFS = 4, MULIB_CONTAINER_CAP_ISOLATION = 8,
  MULIB_CONTAINER_CAP_BUDGET = 16
};
/** @brief Backend contract. prepare publishes one owned handle on success and
 * cleans up on failure. poll returns AGAIN or DONE and writes exit_code on DONE.
 * poll also observes exits while paused, without advancing paused execution.
 * All other operations return zero on success or negative CT errors. destroy
 * retains ownership on failure. Callbacks must not reenter the same runtime. */
typedef struct mulib_container_runtime_ops
{
  uint32_t capabilities;
  int (*prepare) (void *context, const mulib_container_spec *spec,
                  const mulib_rootfs_t *rootfs, void **handle);
  int (*start) (void *handle);
  int (*poll) (void *handle, size_t budget, int *exit_code);
  int (*signal) (void *handle, unsigned signal_number);
  int (*pause) (void *handle, int paused);
  int (*destroy) (void *handle);
} mulib_container_runtime_ops;
/** @brief Owns a backend handle; borrows container/rootfs for its lifetime. */
typedef struct mulib_container_runtime
{
  mulib_container *container;
  const mulib_rootfs_t *rootfs;
  mulib_container_runtime_ops ops;
  void *handle;
  uint64_t polls, budget_submitted;
  int prepared, busy, last_error, deadline_sent;
} mulib_container_runtime;
/** @brief Prepare resources for a CREATED record. Output unchanged on failure. */
int mulib_container_runtime_init (mulib_container_runtime *runtime,
                                  mulib_container *container,
                                  const mulib_rootfs_t *rootfs,
                                  const mulib_container_runtime_ops *ops,
                                  void *context);
/** @brief Start or restart, committing state only after backend success. */
int mulib_container_runtime_start (mulib_container_runtime *runtime, uint64_t now);
/** @brief Submit a positive work budget. Deadline sends signal 9 once; successful
 * signal delivery is not reported as process exit until poll confirms it. */
int mulib_container_runtime_step (mulib_container_runtime *runtime,
                                  size_t budget, uint64_t now);
/** @brief Deliver a signal in RUNNING/PAUSED states; this does not imply exit. */
int mulib_container_runtime_signal (mulib_container_runtime *runtime, unsigned signal_number);
/** @brief Pause/resume after backend acknowledgement. */
int mulib_container_runtime_pause (mulib_container_runtime *runtime, int paused,
                                   uint64_t now);
/** @brief Destroy only an inactive runtime, then mark metadata DELETED.
 * Failure retains ownership. Zero-initialized/already-closed runtimes succeed. */
int mulib_container_runtime_close (mulib_container_runtime *runtime, uint64_t now);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_RUNTIME) && !defined(MULIB_CONTAINER_RUNTIME_IMPLEMENTED)
#define MULIB_CONTAINER_RUNTIME_IMPLEMENTED
static int
mulib_container_runtime_ready_ (mulib_container_runtime *r)
{
  if (!r || !r->prepared || !r->container) return MULIB_CT_ESTATE;
  if (r->busy) return MULIB_CT_EBUSY;
  if (r->container->generation == UINT64_MAX) return MULIB_CT_ELIMIT;
  return 0;
}
int
mulib_container_runtime_init (mulib_container_runtime *r, mulib_container *c,
                              const mulib_rootfs_t *root,
                              const mulib_container_runtime_ops *ops, void *context)
{
  mulib_container_runtime next;
  int rc;
  if (!r || !c || !ops || !ops->prepare || !ops->start || !ops->poll || !ops->destroy
      || ((ops->capabilities & MULIB_CONTAINER_CAP_PAUSE) && !ops->pause)
      || ((ops->capabilities & MULIB_CONTAINER_CAP_SIGNAL) && !ops->signal))
    return MULIB_CT_EINVAL;
  if (c->state != MULIB_CONTAINER_CREATED) return MULIB_CT_ESTATE;
  rc = mulib_container_spec_validate (&c->spec);
  if (rc) return rc;
  if (c->spec.limits.wall_deadline && !ops->signal) return MULIB_CT_ENOTSUP;
  memset (&next, 0, sizeof (next));
  next.container = c; next.rootfs = root; next.ops = *ops;
  rc = ops->prepare (context, &c->spec, root, &next.handle);
  if (rc) return rc < 0 ? rc : MULIB_CT_EIO;
  next.prepared = 1; *r = next;
  return 0;
}
int
mulib_container_runtime_start (mulib_container_runtime *r, uint64_t now)
{
  int rc = mulib_container_runtime_ready_ (r);
  if (rc) return rc;
  if (r->container->state != MULIB_CONTAINER_CREATED
      && r->container->state != MULIB_CONTAINER_EXITED) return MULIB_CT_ESTATE;
  rc = mulib_container_can_transition (r->container, MULIB_CONTAINER_RUNNING);
  if (rc) return rc;
  r->busy = 1; rc = r->ops.start (r->handle); r->busy = 0;
  if (rc > 0) rc = MULIB_CT_EIO;
  r->last_error = rc;
  if (rc) return rc;
  r->deadline_sent = 0;
  return mulib_container_transition (r->container, MULIB_CONTAINER_RUNNING, now, 0);
}
int
mulib_container_runtime_signal (mulib_container_runtime *r, unsigned sig)
{
  int rc = mulib_container_runtime_ready_ (r);
  if (rc) return rc;
  if (!sig || sig > 64) return MULIB_CT_EINVAL;
  if (r->container->state != MULIB_CONTAINER_RUNNING
      && r->container->state != MULIB_CONTAINER_PAUSED) return MULIB_CT_ESTATE;
  if (!r->ops.signal) return MULIB_CT_ENOTSUP;
  r->busy = 1; rc = r->ops.signal (r->handle, sig); r->busy = 0;
  if (rc > 0) rc = MULIB_CT_EIO;
  r->last_error = rc;
  return rc;
}
int
mulib_container_runtime_step (mulib_container_runtime *r, size_t budget, uint64_t now)
{
  int code = 0, rc = mulib_container_runtime_ready_ (r);
  if (rc) return rc;
  if (!budget) return MULIB_CT_EINVAL;
  if (r->container->state == MULIB_CONTAINER_EXITED) return MULIB_CT_DONE;
  if (r->container->state != MULIB_CONTAINER_RUNNING
      && r->container->state != MULIB_CONTAINER_PAUSED) return MULIB_CT_ESTATE;
  if (!r->deadline_sent && r->container->spec.limits.wall_deadline
      && now >= r->container->spec.limits.wall_deadline)
    {
      rc = mulib_container_runtime_signal (r, 9);
      if (rc) return rc;
      r->deadline_sent = 1;
    }
  r->busy = 1; rc = r->ops.poll (r->handle, budget, &code); r->busy = 0;
  r->polls = mulib_ct_counter_add (r->polls, 1);
  r->budget_submitted = mulib_ct_counter_add (r->budget_submitted, budget);
  if (rc == MULIB_CT_DONE)
    {
      int state_rc = mulib_container_transition (r->container, MULIB_CONTAINER_EXITED, now, code);
      if (state_rc) return state_rc;
    }
  else if (rc >= 0 && rc != MULIB_CT_AGAIN) rc = MULIB_CT_EIO;
  r->last_error = rc < 0 ? rc : 0;
  return rc;
}
int
mulib_container_runtime_pause (mulib_container_runtime *r, int paused, uint64_t now)
{
  mulib_container_state expected = paused ? MULIB_CONTAINER_RUNNING : MULIB_CONTAINER_PAUSED;
  int rc = mulib_container_runtime_ready_ (r);
  if (rc) return rc;
  if (r->container->state != expected) return MULIB_CT_ESTATE;
  if (!r->ops.pause) return MULIB_CT_ENOTSUP;
  rc = mulib_container_can_transition (r->container,
      paused ? MULIB_CONTAINER_PAUSED : MULIB_CONTAINER_RUNNING);
  if (rc) return rc;
  r->busy = 1; rc = r->ops.pause (r->handle, paused != 0); r->busy = 0;
  if (rc > 0) rc = MULIB_CT_EIO;
  r->last_error = rc;
  if (rc) return rc;
  return mulib_container_transition (r->container,
      paused ? MULIB_CONTAINER_PAUSED : MULIB_CONTAINER_RUNNING, now, 0);
}
int
mulib_container_runtime_close (mulib_container_runtime *r, uint64_t now)
{
  int rc;
  if (!r) return MULIB_CT_EINVAL;
  if (!r->prepared) return 0;
  rc = mulib_container_runtime_ready_ (r);
  if (rc) return rc;
  if (r->container->state != MULIB_CONTAINER_CREATED
      && r->container->state != MULIB_CONTAINER_EXITED) return MULIB_CT_EBUSY;
  rc = mulib_container_can_transition (r->container, MULIB_CONTAINER_DELETED);
  if (rc) return rc;
  r->busy = 1; rc = r->ops.destroy (r->handle); r->busy = 0;
  if (rc > 0) rc = MULIB_CT_EIO;
  r->last_error = rc;
  if (rc) return rc;
  rc = mulib_container_transition (r->container, MULIB_CONTAINER_DELETED, now, 0);
  memset (r, 0, sizeof (*r));
  return rc;
}
#endif
