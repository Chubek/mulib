/** @file mulib-container-kvmboost.h
 * @brief Owning single-vCPU KVM backend with explicit guest/device callbacks.
 * Define MULIB_IMPL_CONTAINER_KVMBOOST, KVMCONTAINER and CONTAINERBOOST deps. */
#ifndef MULIB_CONTAINER_KVMBOOST_H
#define MULIB_CONTAINER_KVMBOOST_H
#include "mulib-containerboost.h"
#include "mulib-kvmcontainer.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief configure registers borrowed RAM/devices; reset sets initial CPU state.
 * exit handles one native KVM exit and returns AGAIN/DONE/negative CT error.
 * User RAM must outlive runtime close. Callbacks and context remain borrowed. */
typedef struct mulib_container_kvmboost
{
  const char *device_path;
  void *user;
  int (*configure) (void *, mulib_kvmcontainer_t *, const mulib_container_spec *,
                    const mulib_rootfs_t *);
  int (*reset) (void *, mulib_kvmcontainer_t *);
  int (*exit) (void *, mulib_kvmcontainer_t *, uint32_t, int *);
  uint64_t exit_limit;
} mulib_container_kvmboost;
/** @brief Initialize callbacks to NULL and exit quota to one million. */
void mulib_container_kvmboost_init (mulib_container_kvmboost *config);
/** @brief Open/close the KVM device to probe actual availability. */
int mulib_container_kvmboost_probe (const char *device_path);
/** @brief Return lifecycle operations; CPU/memory/file limits need guest policy
 * and are rejected here. Work budget counts exits, not guest instructions. */
mulib_container_runtime_ops mulib_container_kvmboost_ops (void);
/** @brief Create registry descriptor. configure/reset/exit are all required. */
int mulib_container_kvmboost_backend (mulib_container_kvmboost *config,
                                      const char *name, int priority,
                                      mulib_containerboost_backend *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_KVMBOOST) && !defined(MULIB_CONTAINER_KVMBOOST_IMPLEMENTED)
#define MULIB_CONTAINER_KVMBOOST_IMPLEMENTED
typedef struct mulib_container_kvmboost_handle_
{
  mulib_kvmcontainer_t machine;
  mulib_container_kvmboost config;
  uint64_t exits;
  int stopped, paused, code;
} mulib_container_kvmboost_handle_;
void mulib_container_kvmboost_init (mulib_container_kvmboost *c)
{
  if (c) { memset (c, 0, sizeof (*c)); c->exit_limit = 1000000; }
}
int mulib_container_kvmboost_probe (const char *path)
{
  mulib_kvm_t device = MULIB_KVM_INIT;
  int rc = mulib_kvm_open (&device, path);
  if (rc) return rc == MULIB_KVM_ENOTSUP ? MULIB_CT_ENOTSUP : MULIB_CT_EIO;
  return mulib_kvm_close (&device) ? MULIB_CT_EIO : 0;
}
static int
mulib_container_kvmboost_prepare_ (void *context, const mulib_container_spec *s,
                                   const mulib_rootfs_t *root, void **out)
{
  mulib_container_kvmboost *c = (mulib_container_kvmboost *)context;
  mulib_container_kvmboost_handle_ *h;
  int rc;
  if (!c || !out || !c->configure || !c->reset || !c->exit) return MULIB_CT_EINVAL;
  if (s->limits.memory_bytes || s->limits.cpu_budget || s->limits.open_files
      || s->limits.processes) return MULIB_CT_ENOTSUP;
  h = (mulib_container_kvmboost_handle_ *)MULIB_CT_MALLOC (sizeof (*h));
  if (!h) return MULIB_CT_ENOMEM;
  memset (h, 0, sizeof (*h)); h->config = *c;
  mulib_kvmcontainer_init (&h->machine);
  rc = mulib_kvmcontainer_open (&h->machine, c->device_path);
  if (rc) { MULIB_CT_FREE (h); return rc == MULIB_KVM_ENOTSUP ? MULIB_CT_ENOTSUP : MULIB_CT_EIO; }
  rc = c->configure (c->user, &h->machine, s, root);
  if (rc)
    { (void)mulib_kvmcontainer_close (&h->machine); MULIB_CT_FREE (h); return rc < 0 ? rc : MULIB_CT_EIO; }
  *out = h;
  return 0;
}
static int mulib_container_kvmboost_start_ (void *handle)
{
  mulib_container_kvmboost_handle_ *h = (mulib_container_kvmboost_handle_ *)handle;
  int rc = h->config.reset (h->config.user, &h->machine);
  if (!rc) { h->stopped = h->paused = h->code = 0; h->exits = 0; }
  return rc;
}
static int mulib_container_kvmboost_poll_ (void *handle, size_t budget, int *code)
{
  mulib_container_kvmboost_handle_ *h = (mulib_container_kvmboost_handle_ *)handle;
  size_t i;
  if (h->stopped) { *code = h->code; return MULIB_CT_DONE; }
  if (h->paused) return MULIB_CT_AGAIN;
  for (i = 0; i < budget; ++i)
    {
      uint32_t reason;
      int rc;
      if (h->exits >= h->config.exit_limit)
        { h->stopped = 1; *code = h->code = 124; return MULIB_CT_DONE; }
      rc = mulib_kvm_run (&h->machine.cpu, &reason);
      if (rc == MULIB_KVM_INTERRUPTED) return MULIB_CT_AGAIN;
      if (rc) return MULIB_CT_EIO;
      ++h->exits;
      rc = h->config.exit (h->config.user, &h->machine, reason, &h->code);
      if (rc == MULIB_CT_DONE) { h->stopped = 1; *code = h->code; return rc; }
      if (rc != MULIB_CT_AGAIN) return rc < 0 ? rc : MULIB_CT_EIO;
    }
  return MULIB_CT_AGAIN;
}
static int mulib_container_kvmboost_signal_ (void *handle, unsigned sig)
{
  mulib_container_kvmboost_handle_ *h = (mulib_container_kvmboost_handle_ *)handle;
  if (sig != 9 && sig != 15) return MULIB_CT_ENOTSUP;
  h->stopped = 1; h->code = 128 + (int)sig;
  return 0;
}
static int mulib_container_kvmboost_pause_ (void *handle, int paused)
{ ((mulib_container_kvmboost_handle_ *)handle)->paused = paused; return 0; }
static int mulib_container_kvmboost_destroy_ (void *handle)
{
  mulib_container_kvmboost_handle_ *h = (mulib_container_kvmboost_handle_ *)handle;
  if (!h) return 0;
  if (mulib_kvmcontainer_close (&h->machine)) return MULIB_CT_EIO;
  MULIB_CT_FREE (h);
  return 0;
}
mulib_container_runtime_ops mulib_container_kvmboost_ops (void)
{
  mulib_container_runtime_ops o;
  memset (&o, 0, sizeof (o)); o.capabilities = MULIB_CONTAINER_CAP_PAUSE | MULIB_CONTAINER_CAP_SIGNAL;
  o.prepare = mulib_container_kvmboost_prepare_; o.start = mulib_container_kvmboost_start_;
  o.poll = mulib_container_kvmboost_poll_; o.signal = mulib_container_kvmboost_signal_;
  o.pause = mulib_container_kvmboost_pause_; o.destroy = mulib_container_kvmboost_destroy_;
  return o;
}
static int mulib_container_kvmboost_available_ (void *context)
{
  return mulib_container_kvmboost_probe (((mulib_container_kvmboost *)context)->device_path) == 0;
}
int
mulib_container_kvmboost_backend (mulib_container_kvmboost *c, const char *name,
                                 int priority, mulib_containerboost_backend *out)
{
  mulib_containerboost_backend b;
  if (!c || !out || !mulib_ct_id_valid (name) || !c->configure || !c->reset || !c->exit)
    return MULIB_CT_EINVAL;
  memset (&b, 0, sizeof (b)); (void)mulib_ct_text_copy (b.name, sizeof (b.name), name);
  b.kind = MULIB_CONTAINERBOOST_KVM;
  b.architecture_mask = (1U << MULIB_IMGFORMAT_AMD64) | (1U << MULIB_IMGFORMAT_ARM64);
  b.priority = priority; b.context = c; b.ops = mulib_container_kvmboost_ops ();
  b.available = mulib_container_kvmboost_available_; *out = b;
  return 0;
}
#endif
