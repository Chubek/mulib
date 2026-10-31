/** @file mulib-container-qvmboost.h
 * @brief Portable quota-VM execution backend with resumable instruction budgets.
 * Define MULIB_IMPL_CONTAINER_QVMBOOST and CONTAINERBOOST dependencies. */
#ifndef MULIB_CONTAINER_QVMBOOST_H
#define MULIB_CONTAINER_QVMBOOST_H
#include "mulib-containerboost.h"
#include "include-mulib-qvm.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Borrowed program configuration; prepare deep-copies code and allocates RAM. */
typedef struct mulib_container_qvmboost
{
  const mulib_qvm_instruction *code;
  size_t count, memory_bytes, max_bytes;
  uint64_t step_limit;
  mulib_qvm_host host;
  void *user;
} mulib_container_qvmboost;
/** @brief Default quotas: 1 MiB RAM, 16 MiB total allocation, one million steps. */
void mulib_container_qvmboost_init (mulib_container_qvmboost *config,
                                   const mulib_qvm_instruction *code, size_t count);
/** @brief Return runtime operations. The backend implements pause, signal 9/15,
 * memory and CPU quotas. Rootfs, argv/env and identity are application metadata;
 * host calls must explicitly implement any filesystem/process services. */
mulib_container_runtime_ops mulib_container_qvmboost_ops (void);
/** @brief Construct a registry descriptor borrowing config. */
int mulib_container_qvmboost_backend (mulib_container_qvmboost *config,
                                      const char *name, int priority,
                                      mulib_containerboost_backend *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_QVMBOOST) && !defined(MULIB_CONTAINER_QVMBOOST_IMPLEMENTED)
#define MULIB_CONTAINER_QVMBOOST_IMPLEMENTED
typedef struct mulib_container_qvmboost_handle_
{
  mulib_qvm vm;
  int started, paused;
} mulib_container_qvmboost_handle_;
void
mulib_container_qvmboost_init (mulib_container_qvmboost *c,
                               const mulib_qvm_instruction *code, size_t count)
{
  if (!c) return;
  memset (c, 0, sizeof (*c)); c->code = code; c->count = count;
  c->memory_bytes = 1024U * 1024U; c->max_bytes = 16U * 1024U * 1024U;
  c->step_limit = 1000000;
}
static int
mulib_container_qvmboost_prepare_ (void *context, const mulib_container_spec *s,
                                   const mulib_rootfs_t *root, void **out)
{
  mulib_container_qvmboost *c = (mulib_container_qvmboost *)context;
  mulib_container_qvmboost_handle_ *h;
  size_t limit;
  uint64_t steps;
  int rc;
  (void)root;
  if (!c || !out) return MULIB_CT_EINVAL;
  if (s->limits.processes > 1 || s->limits.open_files) return MULIB_CT_ENOTSUP;
  limit = c->max_bytes; steps = c->step_limit;
  if (s->limits.memory_bytes && s->limits.memory_bytes < limit) limit = (size_t)s->limits.memory_bytes;
  if (s->limits.cpu_budget && s->limits.cpu_budget < steps) steps = s->limits.cpu_budget;
  h = (mulib_container_qvmboost_handle_ *)MULIB_CT_MALLOC (sizeof (*h));
  if (!h) return MULIB_CT_ENOMEM;
  memset (h, 0, sizeof (*h));
  rc = mulib_qvm_init (&h->vm, c->code, c->count, c->memory_bytes, limit, steps, c->host, c->user);
  if (rc) { MULIB_CT_FREE (h); return rc; }
  *out = h;
  return 0;
}
static int mulib_container_qvmboost_start_ (void *handle)
{
  mulib_container_qvmboost_handle_ *h = (mulib_container_qvmboost_handle_ *)handle;
  int rc = mulib_qvm_reset (&h->vm);
  if (!rc) { h->started = 1; h->paused = 0; }
  return rc;
}
static int mulib_container_qvmboost_poll_ (void *handle, size_t budget, int *code)
{
  mulib_container_qvmboost_handle_ *h = (mulib_container_qvmboost_handle_ *)handle;
  int rc;
  if (!h->started) return MULIB_CT_ESTATE;
  if (h->paused && !h->vm.halted) return MULIB_CT_AGAIN;
  rc = mulib_qvm_run (&h->vm, budget);
  if (rc < 0) { h->vm.halted = 1; h->vm.exit_code = 125; *code = 125; return MULIB_CT_DONE; }
  if (rc == MULIB_CT_DONE) *code = h->vm.exit_code;
  return rc;
}
static int mulib_container_qvmboost_signal_ (void *handle, unsigned sig)
{
  mulib_container_qvmboost_handle_ *h = (mulib_container_qvmboost_handle_ *)handle;
  if (sig != 9 && sig != 15) return MULIB_CT_ENOTSUP;
  h->vm.halted = 1; h->vm.fault = 0; h->vm.exit_code = 128 + (int)sig;
  return 0;
}
static int mulib_container_qvmboost_pause_ (void *handle, int paused)
{
  ((mulib_container_qvmboost_handle_ *)handle)->paused = paused;
  return 0;
}
static int mulib_container_qvmboost_destroy_ (void *handle)
{
  mulib_container_qvmboost_handle_ *h = (mulib_container_qvmboost_handle_ *)handle;
  if (h) { mulib_qvm_free (&h->vm); MULIB_CT_FREE (h); }
  return 0;
}
mulib_container_runtime_ops
mulib_container_qvmboost_ops (void)
{
  mulib_container_runtime_ops o;
  memset (&o, 0, sizeof (o));
  o.capabilities = MULIB_CONTAINER_CAP_PAUSE | MULIB_CONTAINER_CAP_SIGNAL | MULIB_CONTAINER_CAP_BUDGET;
  o.prepare = mulib_container_qvmboost_prepare_; o.start = mulib_container_qvmboost_start_;
  o.poll = mulib_container_qvmboost_poll_; o.signal = mulib_container_qvmboost_signal_;
  o.pause = mulib_container_qvmboost_pause_; o.destroy = mulib_container_qvmboost_destroy_;
  return o;
}
int
mulib_container_qvmboost_backend (mulib_container_qvmboost *c, const char *name,
                                 int priority, mulib_containerboost_backend *out)
{
  mulib_containerboost_backend b;
  int rc;
  if (!c || !out || !mulib_ct_id_valid (name)) return MULIB_CT_EINVAL;
  rc = mulib_qvm_validate (c->code, c->count);
  if (rc) return rc;
  memset (&b, 0, sizeof (b)); (void)mulib_ct_text_copy (b.name, sizeof (b.name), name);
  b.kind = MULIB_CONTAINERBOOST_QVM; b.architecture_mask = 1U | (1U << MULIB_IMGFORMAT_QVM);
  b.priority = priority; b.context = c; b.ops = mulib_container_qvmboost_ops (); *out = b;
  return 0;
}
#endif
