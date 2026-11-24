/** @file mulib-jsdom.h
 * @brief Native callback bridge for script-visible DOM object/property
 * operations. C11 single-header module. Define MULIB_IMPL_JSDOM in exactly one
 * C translation unit. Callback bridge substrate, not native ECMAScript
 * property descriptors.
 */
#ifndef MULIB_JSDOM_H
#define MULIB_JSDOM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_jsdom_op
  {
    MULIB_JSDOM_GET = 1,
    MULIB_JSDOM_SET = 2,
    MULIB_JSDOM_CALL = 3
  } mulib_jsdom_op_t;
  typedef int (*mulib_jsdom_dispatch_fn) (void *ctx, mulib_jsdom_op_t op,
                                          uint64_t object_id,
                                          const char *property,
                                          const char *input, size_t length,
                                          char *out, size_t cap);
  typedef struct mulib_jsdom
  {
    mulib_jsdom_dispatch_fn dispatch;
    void *ctx;
  } mulib_jsdom_t;
  /** @brief See manual/mulib-jsdom.md for this operation's contract. */
  int mulib_jsdom_invoke (const mulib_jsdom_t *js, mulib_jsdom_op_t op,
                          uint64_t object_id, const char *property,
                          const char *value, size_t n, char *out, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_JSDOM_H */
#ifdef MULIB_IMPL_JSDOM
#ifndef MULIB_IMPL_JSDOM_ONCE
#define MULIB_IMPL_JSDOM_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_jsdom_invoke (const mulib_jsdom_t *j, mulib_jsdom_op_t op, uint64_t id,
                    const char *prop, const char *val, size_t n, char *out,
                    size_t cap)
{
  if (!j || !j->dispatch || op < 1 || op > 3 || !id || !prop || !*prop
      || strlen (prop) > 256 || (!val && n) || !out || !cap)
    return 0;
  char *tmp = calloc (cap, 1);
  if (!tmp)
    return 0;
  int ok = j->dispatch (j->ctx, op, id, prop, val, n, tmp, cap);
  if (ok)
    {
      if (!memchr (tmp, 0, cap))
        ok = 0;
      else
        memmove (out, tmp, strlen (tmp) + 1);
    }
  free (tmp);
  return ok != 0;
}
#endif
#endif
