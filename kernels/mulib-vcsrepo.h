/** @file mulib-vcsrepo.h
 * @brief Bounded owned Git-object repository and compare-and-swap references.
 * Define MULIB_IMPL_VCSREPO and MULIB_IMPL_GIT once. In-memory, not a .git
 * directory implementation; no worktree/index or concurrent synchronization.
 */
#ifndef MULIB_VCSREPO_H
#define MULIB_VCSREPO_H
#include "mulib-git.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vcsrepo_object
  {
    mulib_git_oid_t id;
    char type[7];
    unsigned char *data;
    size_t length;
  } mulib_vcsrepo_object_t;
  typedef struct mulib_vcsrepo_ref
  {
    char name[256];
    mulib_git_oid_t id;
  } mulib_vcsrepo_ref_t;
  typedef struct mulib_vcsrepo
  {
    mulib_vcsrepo_object_t *objects;
    mulib_vcsrepo_ref_t *refs;
    size_t count, ref_count, max_objects, max_refs, bytes, max_bytes;
  } mulib_vcsrepo_t;
  /** @brief Initialize empty storage, 1 success/0 invalid or allocation
   * failure. */
  int mulib_vcsrepo_init (mulib_vcsrepo_t *, size_t max_objects,
                          size_t max_refs, size_t max_bytes);
  /** @brief Copy/deduplicate an object. 1 success, 0 invalid/allocation/limit,
   * -1 SHA-1 collision (same id but different content/type). */
  int mulib_vcsrepo_put (mulib_vcsrepo_t *, const char *type, const void *,
                         size_t, mulib_git_oid_t *);
  /** @brief Borrow immutable object until repository free; NULL if absent. */
  const mulib_vcsrepo_object_t *mulib_vcsrepo_get (const mulib_vcsrepo_t *,
                                                   const mulib_git_oid_t *);
  /** @brief Read a named reference; 1 found, 0 absent/invalid. */
  int mulib_vcsrepo_ref_get (const mulib_vcsrepo_t *, const char *,
                             mulib_git_oid_t *);
  /** @brief Set refs/... name to an existing object. expected=NULL requires
   * absence; otherwise requires equal existing id. 1 updated, 0 invalid/limit,
   * -1 compare failed. Ref names follow the documented strict Git subset. */
  int mulib_vcsrepo_ref_update (mulib_vcsrepo_t *, const char *,
                                const mulib_git_oid_t *expected,
                                const mulib_git_oid_t *next);
  /** @brief Free owned objects and references, zero state; NULL-safe. */
  void mulib_vcsrepo_free (mulib_vcsrepo_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VCSREPO) && !defined(MULIB_VCSREPO_IMPLEMENTED)
#define MULIB_VCSREPO_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_vcsrepo_init (mulib_vcsrepo_t *r, size_t objects, size_t refs,
                    size_t bytes)
{
  mulib_vcsrepo_t v;
  if (!r || !objects || !refs || objects > SIZE_MAX / sizeof (*v.objects)
      || refs > SIZE_MAX / sizeof (*v.refs))
    return 0;
  memset (&v, 0, sizeof (v));
  v.objects = calloc (objects, sizeof (*v.objects));
  v.refs = calloc (refs, sizeof (*v.refs));
  if (!v.objects || !v.refs)
    {
      free (v.objects);
      free (v.refs);
      return 0;
    }
  v.max_objects = objects;
  v.max_refs = refs;
  v.max_bytes = bytes;
  *r = v;
  return 1;
}
const mulib_vcsrepo_object_t *
mulib_vcsrepo_get (const mulib_vcsrepo_t *r, const mulib_git_oid_t *id)
{
  if (!r || !id)
    return NULL;
  for (size_t i = 0; i < r->count; ++i)
    if (!memcmp (r->objects[i].id.bytes, id->bytes, 20))
      return r->objects + i;
  return NULL;
}
int
mulib_vcsrepo_put (mulib_vcsrepo_t *r, const char *type, const void *p,
                   size_t n, mulib_git_oid_t *out)
{
  mulib_git_oid_t id;
  const mulib_vcsrepo_object_t *old;
  unsigned char *copy;
  if (!r || !r->objects || !out || !mulib_git_object_id (type, p, n, &id))
    return 0;
  old = mulib_vcsrepo_get (r, &id);
  if (old)
    {
      if (strcmp (old->type, type) || old->length != n
          || (n && memcmp (old->data, p, n)))
        return -1;
      *out = id;
      return 1;
    }
  if (r->count == r->max_objects || n > r->max_bytes - r->bytes)
    return 0;
  copy = malloc (n ? n : 1);
  if (!copy)
    return 0;
  if (n)
    memcpy (copy, p, n);
  r->objects[r->count].id = id;
  strcpy (r->objects[r->count].type, type);
  r->objects[r->count].data = copy;
  r->objects[r->count++].length = n;
  r->bytes += n;
  *out = id;
  return 1;
}
int
mulib_vcsrepo_ref_get (const mulib_vcsrepo_t *r, const char *name,
                       mulib_git_oid_t *out)
{
  if (!r || !name || !out)
    return 0;
  for (size_t i = 0; i < r->ref_count; ++i)
    if (!strcmp (r->refs[i].name, name))
      {
        *out = r->refs[i].id;
        return 1;
      }
  return 0;
}
static int
mulib_vcsrepo_ref_valid_ (const char *s)
{
  size_t n, start = 0;
  if (!s || strncmp (s, "refs/", 5))
    return 0;
  n = strlen (s);
  if (n < 6 || n > 255 || s[n - 1] == '/' || s[n - 1] == '.'
      || strstr (s, "..") || strstr (s, "@{"))
    return 0;
  for (size_t i = 0; i <= n; ++i)
    {
      unsigned char c = (unsigned char)s[i];
      if (c && (c <= 32 || c == 127 || strchr ("~^:?*[\\", c)))
        return 0;
      if (c == '/' || !c)
        {
          if (i == start || s[start] == '.'
              || (i - start >= 5 && !memcmp (s + i - 5, ".lock", 5)))
            return 0;
          start = i + 1;
        }
    }
  return 1;
}
int
mulib_vcsrepo_ref_update (mulib_vcsrepo_t *r, const char *name,
                          const mulib_git_oid_t *expected,
                          const mulib_git_oid_t *next)
{
  size_t i;
  if (!r || !r->refs || !mulib_vcsrepo_ref_valid_ (name)
      || !mulib_vcsrepo_get (r, next))
    return 0;
  for (i = 0; i < r->ref_count && strcmp (r->refs[i].name, name); ++i)
    {
    }
  if (i == r->ref_count)
    {
      if (expected)
        return -1;
      if (i == r->max_refs)
        return 0;
      strcpy (r->refs[i].name, name);
      ++r->ref_count;
    }
  else if (!expected || memcmp (expected->bytes, r->refs[i].id.bytes, 20))
    return -1;
  r->refs[i].id = *next;
  return 1;
}
void
mulib_vcsrepo_free (mulib_vcsrepo_t *r)
{
  if (!r)
    return;
  for (size_t i = 0; i < r->count; ++i)
    free (r->objects[i].data);
  free (r->objects);
  free (r->refs);
  memset (r, 0, sizeof (*r));
}
#endif
