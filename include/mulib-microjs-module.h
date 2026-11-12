/** @file mulib-microjs-module.h
 * @brief Owned source-module registry with immutable lookup by specifier.
 * C11 single-header module. Define MULIB_IMPL_MICROJS_MODULE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_MICROJS_MODULE_H
#define MULIB_MICROJS_MODULE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_microjs_module
  {
    char *name, *source;
    size_t source_length;
  } mulib_microjs_module_t;
  typedef struct mulib_microjs_modules
  {
    mulib_microjs_module_t *items;
    size_t len, cap, limit;
  } mulib_microjs_modules_t;
  /** @brief See manual/mulib-microjs-module.md for this operation's contract.
   */
  int mulib_microjs_module_register (mulib_microjs_modules_t *m,
                                     const char *name, const char *source,
                                     size_t length);
  const mulib_microjs_module_t *
  /** @brief See manual/mulib-microjs-module.md for this operation's contract.
   */
  mulib_microjs_module_find (const mulib_microjs_modules_t *m,
                             const char *name);
  /** @brief See manual/mulib-microjs-module.md for this operation's contract.
   */
  void mulib_microjs_modules_free (mulib_microjs_modules_t *m);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MICROJS_MODULE_H */
#ifdef MULIB_IMPL_MICROJS_MODULE
#ifndef MULIB_IMPL_MICROJS_MODULE_ONCE
#define MULIB_IMPL_MICROJS_MODULE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const mulib_microjs_module_t *
mulib_microjs_module_find (const mulib_microjs_modules_t *m, const char *name)
{
  if (!m || !name)
    return NULL;
  for (size_t i = 0; i < m->len; i++)
    if (!strcmp (m->items[i].name, name))
      return &m->items[i];
  return NULL;
}
int
mulib_microjs_module_register (mulib_microjs_modules_t *m, const char *name,
                               const char *src, size_t n)
{
  if (!m || !name || !*name || (!src && n) || strlen (name) > 512
      || n > 8 * 1024 * 1024
      || (n && !mulib_utf8_validate ((const unsigned char *)src, n))
      || mulib_microjs_module_find (m, name)
      || (m->limit && m->len >= m->limit))
    return 0;
  mulib_microjs_module_t z = { mulib_shylock_dup (name, strlen (name)),
                               mulib_shylock_dup (src, n), n };
  if (!z.name || !z.source)
    {
      free (z.name);
      free (z.source);
      return 0;
    }
  if (m->len == m->cap)
    {
      size_t c = m->cap ? 2 * m->cap : 16;
      if (c < m->cap || c > SIZE_MAX / sizeof z)
        {
          free (z.name);
          free (z.source);
          return 0;
        }
      if (m->limit && c > m->limit)
        c = m->limit;
      void *p = realloc (m->items, c * sizeof z);
      if (!p)
        {
          free (z.name);
          free (z.source);
          return 0;
        }
      m->items = p;
      m->cap = c;
    }
  m->items[m->len++] = z;
  return 1;
}
void
mulib_microjs_modules_free (mulib_microjs_modules_t *m)
{
  if (m)
    {
      for (size_t i = 0; i < m->len; i++)
        {
          free (m->items[i].name);
          free (m->items[i].source);
        }
      free (m->items);
      memset (m, 0, sizeof *m);
    }
}
#endif
#endif
