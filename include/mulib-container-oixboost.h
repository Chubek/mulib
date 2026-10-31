/** @file mulib-container-oixboost.h
 * @brief OIX guest image compilation, console I/O and container execution adapter.
 * Define MULIB_IMPL_CONTAINER_OIXBOOST plus OIX/CONTAINERBOOST dependencies. */
#ifndef MULIB_CONTAINER_OIXBOOST_H
#define MULIB_CONTAINER_OIXBOOST_H
#include "mulib-containerboost.h"
#include "mulib-oix.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_container_oixboost
{
  const void *image;
  size_t image_length;
  uint64_t exit_limit;
  mulib_oix_output_fn output;
  void *user;
} mulib_container_oixboost;
/** @brief Borrow a precompiled image; default exit quota is one million. */
void mulib_container_oixboost_init (mulib_container_oixboost *config,
                                   const void *image, size_t length);
/** @brief Compile regular rootfs files plus caller tasks into a native OIX image.
 * Directory records are omitted; file count/name/image limits follow OIX. */
int mulib_container_oixboost_compile (const mulib_rootfs_t *root,
    const mulib_oix_config_t *config, mulib_oix_image_t *out, mulib_oix_error_t *error);
/** @brief Operations own private image bytes/VM; native KVM availability is required. */
mulib_container_runtime_ops mulib_container_oixboost_ops (void);
/** @brief Create a descriptor after structural validation of the supplied image. */
int mulib_container_oixboost_backend (mulib_container_oixboost *config,
    const char *name, int priority, mulib_containerboost_backend *out);
/** @brief Copy console input into a running or paused adapter; partial writes use AGAIN. */
int mulib_container_oixboost_feed (mulib_container_runtime *runtime,
                                   const void *data, size_t length, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_OIXBOOST) && !defined(MULIB_CONTAINER_OIXBOOST_IMPLEMENTED)
#define MULIB_CONTAINER_OIXBOOST_IMPLEMENTED
typedef struct mulib_container_oixboost_handle_
{
  mulib_ct_blob image;
  mulib_oix_vm_t *vm;
  mulib_container_oixboost config;
  uint64_t exits;
  int started, stopped, paused, code;
} mulib_container_oixboost_handle_;
void
mulib_container_oixboost_init (mulib_container_oixboost *c, const void *image, size_t n)
{
  if (!c) return;
  memset (c, 0, sizeof (*c)); c->image = image; c->image_length = n; c->exit_limit = 1000000;
}
int
mulib_container_oixboost_compile (const mulib_rootfs_t *root,
    const mulib_oix_config_t *config, mulib_oix_image_t *out, mulib_oix_error_t *error)
{
  mulib_oix_config_t c;
  mulib_oix_file_t files[MULIB_OIX_MAX_FILES];
  size_t i, count = 0;
  if (!root || !config || !out || !root->storage.max_nodes) return MULIB_CT_EINVAL;
  if (config->file_count) return MULIB_CT_EINVAL;
  c = *config;
  for (i = 0; i < root->storage.count; ++i)
    if (root->storage.nodes[i].kind == MULIB_VFS_FILE)
      {
        const mulib_vfs_mem_node *n = root->storage.nodes + i;
        if (count == MULIB_OIX_MAX_FILES) return MULIB_CT_ELIMIT;
        files[count].name = n->path; files[count].data = n->data;
        files[count++].length = n->length;
      }
  c.files = files; c.file_count = count;
  return mulib_oix_compile (&c, out, error) == MULIB_OIX_OK ? 0 : MULIB_CT_EFORMAT;
}
static int
mulib_container_oixboost_prepare_ (void *context, const mulib_container_spec *s,
                                   const mulib_rootfs_t *root, void **out)
{
  mulib_container_oixboost *c = (mulib_container_oixboost *)context;
  mulib_container_oixboost_handle_ *h;
  mulib_oix_error_t error;
  int rc;
  (void)root;
  if (!c || !out) return MULIB_CT_EINVAL;
  if (s->limits.cpu_budget || s->limits.open_files || s->limits.processes
      || (s->limits.memory_bytes && s->limits.memory_bytes < MULIB_OIX_RAM_BYTES))
    return MULIB_CT_ENOTSUP;
  if (mulib_oix_validate (c->image, c->image_length, NULL, &error) != MULIB_OIX_OK)
    return MULIB_CT_EFORMAT;
  h = (mulib_container_oixboost_handle_ *)MULIB_CT_MALLOC (sizeof (*h));
  if (!h) return MULIB_CT_ENOMEM;
  memset (h, 0, sizeof (*h)); h->config = *c;
  rc = mulib_ct_blob_assign (&h->image, c->image, c->image_length, 65536);
  if (rc) { MULIB_CT_FREE (h); return rc; }
  h->vm = mulib_oix_vm_create (h->image.data, h->image.length, &error);
  if (!h->vm)
    {
      mulib_ct_blob_free (&h->image); MULIB_CT_FREE (h);
      return error.code == MULIB_OIX_UNSUPPORTED ? MULIB_CT_ENOTSUP : MULIB_CT_EIO;
    }
  *out = h;
  return 0;
}
static int mulib_container_oixboost_start_ (void *handle)
{
  mulib_container_oixboost_handle_ *h = (mulib_container_oixboost_handle_ *)handle;
  if (h->started)
    {
      mulib_oix_vm_t *next = mulib_oix_vm_create (h->image.data, h->image.length, NULL);
      if (!next) return MULIB_CT_EIO;
      mulib_oix_vm_destroy (h->vm); h->vm = next;
    }
  h->started = 1; h->stopped = h->paused = h->code = 0; h->exits = 0;
  return 0;
}
static int mulib_container_oixboost_poll_ (void *handle, size_t budget, int *code)
{
  mulib_container_oixboost_handle_ *h = (mulib_container_oixboost_handle_ *)handle;
  mulib_oix_result_t result;
  mulib_oix_status_t rc;
  if (h->stopped) { *code = h->code; return MULIB_CT_DONE; }
  if (h->paused) return MULIB_CT_AGAIN;
  if (h->exits >= h->config.exit_limit)
    { h->stopped = 1; *code = h->code = 124; return MULIB_CT_DONE; }
  if (budget > h->config.exit_limit - h->exits) budget = (size_t)(h->config.exit_limit - h->exits);
  memset (&result, 0, sizeof (result));
  rc = mulib_oix_vm_run (h->vm, budget, h->config.output, h->config.user, &result, NULL);
  h->exits = result.exits;
  if (result.halted)
    { h->stopped = 1; *code = h->code = (int)result.exit_code; return MULIB_CT_DONE; }
  if (rc == MULIB_OIX_LIMIT || rc == MULIB_OIX_INTERRUPTED || rc == MULIB_OIX_AGAIN)
    return MULIB_CT_AGAIN;
  return rc == MULIB_OIX_OK ? MULIB_CT_AGAIN : MULIB_CT_EIO;
}
static int mulib_container_oixboost_signal_ (void *handle, unsigned sig)
{
  mulib_container_oixboost_handle_ *h = (mulib_container_oixboost_handle_ *)handle;
  if (sig != 9 && sig != 15) return MULIB_CT_ENOTSUP;
  h->stopped = 1; h->code = 128 + (int)sig;
  return 0;
}
static int mulib_container_oixboost_pause_ (void *handle, int paused)
{ ((mulib_container_oixboost_handle_ *)handle)->paused = paused; return 0; }
static int mulib_container_oixboost_destroy_ (void *handle)
{
  mulib_container_oixboost_handle_ *h = (mulib_container_oixboost_handle_ *)handle;
  if (h) { mulib_oix_vm_destroy (h->vm); mulib_ct_blob_free (&h->image); MULIB_CT_FREE (h); }
  return 0;
}
mulib_container_runtime_ops mulib_container_oixboost_ops (void)
{
  mulib_container_runtime_ops o;
  memset (&o, 0, sizeof (o)); o.capabilities = MULIB_CONTAINER_CAP_PAUSE | MULIB_CONTAINER_CAP_SIGNAL;
  o.prepare = mulib_container_oixboost_prepare_; o.start = mulib_container_oixboost_start_;
  o.poll = mulib_container_oixboost_poll_; o.signal = mulib_container_oixboost_signal_;
  o.pause = mulib_container_oixboost_pause_; o.destroy = mulib_container_oixboost_destroy_;
  return o;
}
int
mulib_container_oixboost_backend (mulib_container_oixboost *c, const char *name,
                                 int priority, mulib_containerboost_backend *out)
{
  mulib_containerboost_backend b;
  if (!c || !out || !mulib_ct_id_valid (name)) return MULIB_CT_EINVAL;
  if (mulib_oix_validate (c->image, c->image_length, NULL, NULL) != MULIB_OIX_OK)
    return MULIB_CT_EFORMAT;
  memset (&b, 0, sizeof (b)); (void)mulib_ct_text_copy (b.name, sizeof (b.name), name);
  b.kind = MULIB_CONTAINERBOOST_OIX; b.architecture_mask = 1U << MULIB_IMGFORMAT_OIX;
  b.priority = priority; b.context = c; b.ops = mulib_container_oixboost_ops (); *out = b;
  return 0;
}
int
mulib_container_oixboost_feed (mulib_container_runtime *r, const void *data,
                              size_t n, size_t *written)
{
  mulib_container_oixboost_handle_ *h;
  mulib_oix_status_t rc;
  if (written) *written = 0;
  if (!r || !r->prepared || !written || r->ops.poll != mulib_container_oixboost_poll_)
    return MULIB_CT_EINVAL;
  if (r->busy) return MULIB_CT_EBUSY;
  h = (mulib_container_oixboost_handle_ *)r->handle;
  if (!h->started || h->stopped) return MULIB_CT_ESTATE;
  rc = mulib_oix_vm_feed (h->vm, data, n, written, NULL);
  return rc == MULIB_OIX_OK ? 0 : rc == MULIB_OIX_AGAIN ? MULIB_CT_AGAIN : MULIB_CT_EIO;
}
#endif
