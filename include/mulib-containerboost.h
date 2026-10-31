/** @file mulib-containerboost.h
 * @brief Explicit execution-backend registry, capability matching and ranking.
 * Define MULIB_IMPL_CONTAINERBOOST and CONTAINER_RUNTIME dependencies. */
#ifndef MULIB_CONTAINERBOOST_H
#define MULIB_CONTAINERBOOST_H
#include "mulib-container-runtime.h"
#include "mulib-imgformat.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_containerboost_kind
{
  MULIB_CONTAINERBOOST_CUSTOM = 0, MULIB_CONTAINERBOOST_KVM,
  MULIB_CONTAINERBOOST_OIX, MULIB_CONTAINERBOOST_QVM, MULIB_CONTAINERBOOST_JIT
} mulib_containerboost_kind;
/** @brief Registry owns this value, but borrows context and callback referents. */
typedef struct mulib_containerboost_backend
{
  char name[MULIB_CT_ID_CAP];
  mulib_containerboost_kind kind;
  uint32_t architecture_mask;
  int priority;
  mulib_container_runtime_ops ops;
  void *context;
  int (*available) (void *context);
} mulib_containerboost_backend;
typedef struct mulib_containerboost
{
  mulib_containerboost_backend *backends;
  size_t count, limit;
} mulib_containerboost;
/** @brief Initialize a bounded registry. */
int mulib_containerboost_init (mulib_containerboost *registry, size_t limit);
/** @brief Free registry storage, never backend contexts. */
void mulib_containerboost_free (mulib_containerboost *registry);
/** @brief Register a copied descriptor; duplicate names fail. */
int mulib_containerboost_register (mulib_containerboost *registry,
                                   const mulib_containerboost_backend *backend);
/** @brief Remove by exact name; existing runtimes keep their copied operations. */
int mulib_containerboost_remove (mulib_containerboost *registry, const char *name);
/** @brief Pick highest priority, stable registration order for ties. available
 * returns positive for available; zero/negative excludes that candidate. */
const mulib_containerboost_backend *mulib_containerboost_select (
    const mulib_containerboost *registry, mulib_imgformat_arch architecture,
    uint32_t required_capabilities, const char *preferred_name);
/** @brief Select and prepare once. Errors propagate without implicit fallback. */
int mulib_containerboost_prepare (const mulib_containerboost *registry,
    mulib_imgformat_arch architecture, uint32_t required_capabilities,
    const char *preferred_name, mulib_container *container,
    const mulib_rootfs_t *rootfs, mulib_container_runtime *runtime);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERBOOST) && !defined(MULIB_CONTAINERBOOST_IMPLEMENTED)
#define MULIB_CONTAINERBOOST_IMPLEMENTED
int
mulib_containerboost_init (mulib_containerboost *r, size_t limit)
{
  if (!r || !limit || limit > SIZE_MAX / sizeof (*r->backends)) return MULIB_CT_EINVAL;
  memset (r, 0, sizeof (*r)); r->limit = limit;
  return 0;
}
void
mulib_containerboost_free (mulib_containerboost *r)
{
  if (r) { MULIB_CT_FREE (r->backends); memset (r, 0, sizeof (*r)); }
}
int
mulib_containerboost_register (mulib_containerboost *r, const mulib_containerboost_backend *b)
{
  size_t i;
  mulib_containerboost_backend copy, *next;
  if (!r || !b || !mulib_ct_id_valid (b->name) || !b->architecture_mask
      || (b->architecture_mask & ~UINT32_C(31)) || b->kind < 0
      || b->kind > MULIB_CONTAINERBOOST_JIT || !b->ops.prepare
       || !b->ops.start || !b->ops.poll || !b->ops.destroy
       || ((b->ops.capabilities & MULIB_CONTAINER_CAP_PAUSE) && !b->ops.pause)
       || ((b->ops.capabilities & MULIB_CONTAINER_CAP_SIGNAL) && !b->ops.signal))
    return MULIB_CT_EINVAL;
  for (i = 0; i < r->count; ++i)
    if (!strcmp (r->backends[i].name, b->name)) return MULIB_CT_EEXIST;
  if (r->count >= r->limit) return MULIB_CT_ELIMIT;
  copy = *b;
  next = (mulib_containerboost_backend *)MULIB_CT_REALLOC (
      r->backends, (r->count + 1) * sizeof (*next));
  if (!next) return MULIB_CT_ENOMEM;
  r->backends = next; next[r->count++] = copy;
  return 0;
}
int
mulib_containerboost_remove (mulib_containerboost *r, const char *name)
{
  size_t i;
  if (!r || !name) return MULIB_CT_EINVAL;
  for (i = 0; i < r->count; ++i)
    if (!strcmp (r->backends[i].name, name))
      {
        memmove (r->backends + i, r->backends + i + 1,
                 (--r->count - i) * sizeof (*r->backends));
        return 0;
      }
  return MULIB_CT_ENOENT;
}
const mulib_containerboost_backend *
mulib_containerboost_select (const mulib_containerboost *r, mulib_imgformat_arch arch,
                             uint32_t caps, const char *name)
{
  const mulib_containerboost_backend *best = NULL;
  size_t i;
  if (!r || arch < 0 || arch > MULIB_IMGFORMAT_QVM) return NULL;
  for (i = 0; i < r->count; ++i)
    {
      const mulib_containerboost_backend *b = r->backends + i;
      if (!(b->architecture_mask & (UINT32_C(1) << arch))
          || (b->ops.capabilities & caps) != caps || (name && strcmp (name, b->name))
          || (b->available && b->available (b->context) <= 0)) continue;
      if (!best || b->priority > best->priority) best = b;
    }
  return best;
}
int
mulib_containerboost_prepare (const mulib_containerboost *r, mulib_imgformat_arch arch,
    uint32_t caps, const char *name, mulib_container *c, const mulib_rootfs_t *root,
    mulib_container_runtime *runtime)
{
  const mulib_containerboost_backend *b;
  if (!r || !c || !runtime) return MULIB_CT_EINVAL;
  b = mulib_containerboost_select (r, arch, caps, name);
  if (!b) return MULIB_CT_ENOTSUP;
  return mulib_container_runtime_init (runtime, c, root, &b->ops, b->context);
}
#endif
