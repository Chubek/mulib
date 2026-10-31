/** @file mulib-container-jitboost.h
 * @brief Native MIR execution backend with resumable budgets and host-call suspension.
 * Define MULIB_IMPL_CONTAINER_JITBOOST and JIT/CONTAINERBOOST dependencies. */
#ifndef MULIB_CONTAINER_JITBOOST_H
#define MULIB_CONTAINER_JITBOOST_H
#include "mulib-containerboost.h"
#include "mulib-jit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_container_jitboost
{
  const mulib_mir_flow_program_t *program;
  const mulib_mir_value_t *arguments;
  size_t argument_count, record_limit;
  uint64_t step_limit;
  mulib_mir_host_fn host;
  void *user;
  int optimize;
} mulib_container_jitboost;
/** @brief Defaults: 65536 records, one million instructions, local optimization. */
void mulib_container_jitboost_init (mulib_container_jitboost *config,
                                   const mulib_mir_flow_program_t *program);
/** @brief Actual native execution; unsupported platforms return ENOTSUP at prepare. */
mulib_container_runtime_ops mulib_container_jitboost_ops (void);
/** @brief Registry descriptor; available probes compiled-in native support. */
int mulib_container_jitboost_backend (mulib_container_jitboost *config,
                                      const char *name, int priority,
                                      mulib_containerboost_backend *out);
/** @brief Complete a suspended host call on this adapter's idle runtime. */
int mulib_container_jitboost_resume (mulib_container_runtime *runtime,
                                     mulib_mir_value_t result);
/** @brief Borrow execution diagnostics: status, instruction count and result. */
int mulib_container_jitboost_inspect (const mulib_container_runtime *runtime,
    mulib_mir_status_t *status, size_t *steps, mulib_mir_value_t *result);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_JITBOOST) && !defined(MULIB_CONTAINER_JITBOOST_IMPLEMENTED)
#define MULIB_CONTAINER_JITBOOST_IMPLEMENTED
typedef struct mulib_container_jitboost_handle_
{
  mulib_jit_flow_t *jit;
  mulib_mir_frame_t frame;
  size_t record_limit;
  uint64_t step_limit;
  int stopped, exit_code, paused;
} mulib_container_jitboost_handle_;
void
mulib_container_jitboost_init (mulib_container_jitboost *c, const mulib_mir_flow_program_t *p)
{
  if (!c) return;
  memset (c, 0, sizeof (*c)); c->program = p;
  c->record_limit = 65536; c->step_limit = 1000000; c->optimize = 1;
}
static int
mulib_container_jitboost_prepare_ (void *context, const mulib_container_spec *s,
                                   const mulib_rootfs_t *root, void **out)
{
  mulib_container_jitboost *c = (mulib_container_jitboost *)context;
  mulib_container_jitboost_handle_ *h;
  mulib_mir_diagnostic_t d;
  int rc;
  (void)root;
  if (!c || !c->program || !out) return MULIB_CT_EINVAL;
  if (!mulib_jit_supported ()) return MULIB_CT_ENOTSUP;
  if (s->limits.memory_bytes || s->limits.open_files || s->limits.processes > 1)
    return MULIB_CT_ENOTSUP;
  if (c->program->signature.result != MULIB_MIR_I64
      && c->program->signature.result != MULIB_MIR_WORD
      && c->program->signature.result != MULIB_MIR_BOOL) return MULIB_CT_ENOTSUP;
  h = (mulib_container_jitboost_handle_ *)MULIB_CT_MALLOC (sizeof (*h));
  if (!h) return MULIB_CT_ENOMEM;
  memset (h, 0, sizeof (*h)); h->record_limit = c->record_limit; h->step_limit = c->step_limit;
  if (s->limits.cpu_budget && s->limits.cpu_budget < h->step_limit) h->step_limit = s->limits.cpu_budget;
  rc = mulib_jit_flow_compile (c->program, c->record_limit, c->optimize, &h->jit, &d);
  if (rc) { MULIB_CT_FREE (h); return rc == MULIB_JIT_ENOTSUP ? MULIB_CT_ENOTSUP : MULIB_CT_EIO; }
  if (mulib_mir_frame_init (&h->frame, mulib_jit_flow_program (h->jit),
      c->arguments, c->argument_count, c->host, c->user, c->record_limit, &d) != MULIB_MIR_OK)
    {
      (void)mulib_jit_flow_destroy (h->jit); MULIB_CT_FREE (h);
      return d.status == MULIB_MIR_NO_MEMORY ? MULIB_CT_ENOMEM : MULIB_CT_EFORMAT;
    }
  *out = h;
  return 0;
}
static int mulib_container_jitboost_start_ (void *handle)
{
  mulib_container_jitboost_handle_ *h = (mulib_container_jitboost_handle_ *)handle;
  mulib_mir_frame_t frame;
  mulib_mir_status_t rc;
  memset (&frame, 0, sizeof (frame));
  rc = mulib_mir_frame_init (&frame, h->frame.program, h->frame.arguments,
      h->frame.program->signature.parameter_count, h->frame.host, h->frame.user,
      h->record_limit, NULL);
  if (rc != MULIB_MIR_OK) return rc == MULIB_MIR_NO_MEMORY ? MULIB_CT_ENOMEM : MULIB_CT_EIO;
  mulib_mir_frame_free (&h->frame); h->frame = frame;
  h->stopped = h->exit_code = h->paused = 0;
  return 0;
}
static int mulib_container_jitboost_poll_ (void *handle, size_t budget, int *code)
{
  mulib_container_jitboost_handle_ *h = (mulib_container_jitboost_handle_ *)handle;
  mulib_mir_status_t rc;
  if (h->stopped) { *code = h->exit_code; return MULIB_CT_DONE; }
  if (h->paused) return MULIB_CT_AGAIN;
  if (h->frame.steps >= h->step_limit)
    { h->stopped = 1; *code = h->exit_code = 124; return MULIB_CT_DONE; }
  if (budget > h->step_limit - h->frame.steps) budget = (size_t)(h->step_limit - h->frame.steps);
  rc = mulib_jit_flow_run (h->jit, &h->frame, budget);
  if (rc == MULIB_MIR_STEP_LIMIT || rc == MULIB_MIR_SUSPENDED) return MULIB_CT_AGAIN;
  h->stopped = 1;
  h->exit_code = rc == MULIB_MIR_FINISHED ? (int)(h->frame.result.word & 255) : 125;
  *code = h->exit_code;
  return MULIB_CT_DONE;
}
static int mulib_container_jitboost_signal_ (void *handle, unsigned sig)
{
  mulib_container_jitboost_handle_ *h = (mulib_container_jitboost_handle_ *)handle;
  if (sig != 9 && sig != 15) return MULIB_CT_ENOTSUP;
  h->stopped = 1; h->exit_code = 128 + (int)sig;
  return 0;
}
static int mulib_container_jitboost_pause_ (void *handle, int paused)
{
  ((mulib_container_jitboost_handle_ *)handle)->paused = paused;
  return 0;
}
static int mulib_container_jitboost_destroy_ (void *handle)
{
  mulib_container_jitboost_handle_ *h = (mulib_container_jitboost_handle_ *)handle;
  if (!h) return 0;
  if (mulib_jit_flow_destroy (h->jit)) return MULIB_CT_EIO;
  mulib_mir_frame_free (&h->frame); MULIB_CT_FREE (h);
  return 0;
}
mulib_container_runtime_ops mulib_container_jitboost_ops (void)
{
  mulib_container_runtime_ops o;
  memset (&o, 0, sizeof (o));
  o.capabilities = MULIB_CONTAINER_CAP_PAUSE | MULIB_CONTAINER_CAP_SIGNAL | MULIB_CONTAINER_CAP_BUDGET;
  o.prepare = mulib_container_jitboost_prepare_; o.start = mulib_container_jitboost_start_;
  o.poll = mulib_container_jitboost_poll_; o.signal = mulib_container_jitboost_signal_;
  o.pause = mulib_container_jitboost_pause_; o.destroy = mulib_container_jitboost_destroy_;
  return o;
}
static int mulib_container_jitboost_available_ (void *context)
{ (void)context; return mulib_jit_supported (); }
int
mulib_container_jitboost_backend (mulib_container_jitboost *c, const char *name,
                                 int priority, mulib_containerboost_backend *out)
{
  mulib_containerboost_backend b;
  if (!c || !out || !mulib_ct_id_valid (name)) return MULIB_CT_EINVAL;
  memset (&b, 0, sizeof (b)); (void)mulib_ct_text_copy (b.name, sizeof (b.name), name);
  b.kind = MULIB_CONTAINERBOOST_JIT; b.architecture_mask = 1U | (1U << MULIB_IMGFORMAT_AMD64);
  b.priority = priority; b.context = c; b.ops = mulib_container_jitboost_ops ();
  b.available = mulib_container_jitboost_available_; *out = b;
  return 0;
}
int
mulib_container_jitboost_resume (mulib_container_runtime *r, mulib_mir_value_t value)
{
  mulib_container_jitboost_handle_ *h;
  if (!r || !r->prepared || r->ops.poll != mulib_container_jitboost_poll_) return MULIB_CT_EINVAL;
  if (r->busy) return MULIB_CT_EBUSY;
  h = (mulib_container_jitboost_handle_ *)r->handle;
  if (h->stopped) return MULIB_CT_ESTATE;
  return mulib_mir_frame_resume (&h->frame, value) == MULIB_MIR_OK ? 0 : MULIB_CT_ESTATE;
}
int
mulib_container_jitboost_inspect (const mulib_container_runtime *r,
    mulib_mir_status_t *status, size_t *steps, mulib_mir_value_t *result)
{
  const mulib_container_jitboost_handle_ *h;
  if (!r || !r->prepared || r->ops.poll != mulib_container_jitboost_poll_) return MULIB_CT_EINVAL;
  if (r->busy) return MULIB_CT_EBUSY;
  h = (const mulib_container_jitboost_handle_ *)r->handle;
  if (status) *status = h->frame.status;
  if (steps) *steps = h->frame.steps;
  if (result) *result = h->frame.result;
  return 0;
}
#endif
