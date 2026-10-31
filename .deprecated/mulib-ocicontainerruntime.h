/** @file mulib-ocicontainerruntime.h
 * @brief Explicit-backend container lifecycle driver, not a bundled OCI
 * runtime. Define MULIB_IMPL_OCICONTAINERRUNTIME plus OCICONTAINER. */
#ifndef MULIB_OCICONTAINERRUNTIME_H
#define MULIB_OCICONTAINERRUNTIME_H
#include "mulib-ocicontainer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Perform action before publication; return 0 only if OS action
   * succeeded. Failure must leave backend state unchanged. No reentrancy or
   * mutation of runtime. */
  typedef int (*mulib_ocicontainerruntime_action_fn) (
      void *context, const mulib_ocicontainer_t *current, int next,
      int exit_code);
  typedef struct mulib_ocicontainerruntime
  {
    mulib_ocicontainer_t container;
    mulib_ocicontainerruntime_action_fn action;
    void *context;
  } mulib_ocicontainerruntime_t;
  /** @brief Initialize CREATED state with mandatory backend. 1 success. */
  int mulib_ocicontainerruntime_init (
      mulib_ocicontainerruntime_t *runtime, const char *id,
      mulib_ocicontainerruntime_action_fn action, void *context);
  /** @brief Validate, call backend, publish transition. 0 success, -1 invalid,
   * backend's nonzero error otherwise. No isolation/security guarantees beyond
   * backend. */
  int mulib_ocicontainerruntime_apply (mulib_ocicontainerruntime_t *runtime,
                                       int next, int exit_code);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCICONTAINERRUNTIME)                                   \
    && !defined(MULIB_OCICONTAINERRUNTIME_IMPLEMENTED)
#define MULIB_OCICONTAINERRUNTIME_IMPLEMENTED
int
mulib_ocicontainerruntime_init (mulib_ocicontainerruntime_t *r, const char *id,
                                mulib_ocicontainerruntime_action_fn f,
                                void *ctx)
{
  mulib_ocicontainerruntime_t v;
  if (!r || !f || !mulib_ocicontainer_create (&v.container, id))
    return 0;
  v.action = f;
  v.context = ctx;
  *r = v;
  return 1;
}
int
mulib_ocicontainerruntime_apply (mulib_ocicontainerruntime_t *r, int next,
                                 int code)
{
  mulib_ocicontainer_t desired;
  int rc;
  if (!r || !r->action)
    return -1;
  desired = r->container;
  if (!mulib_ocicontainer_transition (&desired, next, code))
    return -1;
  rc = r->action (r->context, &r->container, next, code);
  if (rc)
    return rc;
  r->container = desired;
  return 0;
}
#endif
