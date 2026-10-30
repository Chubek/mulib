/** @file mulib-ocicontainer.h
 * @brief OCI-style container identity/lifecycle core, not an isolation engine.
 * Define MULIB_IMPL_OCICONTAINER once. OS actions belong to a runtime backend.
 */
#ifndef MULIB_OCICONTAINER_H
#define MULIB_OCICONTAINER_H
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_OCICONTAINER_CREATED = 1,
    MULIB_OCICONTAINER_RUNNING = 2,
    MULIB_OCICONTAINER_STOPPED = 3,
    MULIB_OCICONTAINER_DELETED = 4
  };
  typedef struct mulib_ocicontainer
  {
    char id[65];
    int state, exit_code;
  } mulib_ocicontainer_t;
  /** @brief Initialize with 1..64 ASCII alnum/underscore/hyphen/dot ID,
   * excluding dot and dot-dot. 1 success. IDs never interpreted as native
   * filesystem paths. */
  int mulib_ocicontainer_create (mulib_ocicontainer_t *c, const char *id);
  /** @brief Validate a transition without mutation; allowed
   * created->running/deleted, running->stopped, stopped->deleted. Deleted is
   * terminal. */
  int mulib_ocicontainer_can_transition (const mulib_ocicontainer_t *c,
                                         int next);
  /** @brief Apply allowed transition; exit_code used only for STOPPED, range
   * 0..255. 1 success, 0 invalid transition/code, state unchanged. */
  int mulib_ocicontainer_transition (mulib_ocicontainer_t *c, int next,
                                     int exit_code);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCICONTAINER)                                          \
    && !defined(MULIB_OCICONTAINER_IMPLEMENTED)
#define MULIB_OCICONTAINER_IMPLEMENTED
#include <stddef.h>
#include <string.h>
int
mulib_ocicontainer_create (mulib_ocicontainer_t *c, const char *s)
{
  size_t i, n;
  mulib_ocicontainer_t t;
  if (!c || !s)
    return 0;
  n = strlen (s);
  if (!n || n > 64 || !strcmp (s, ".") || !strcmp (s, ".."))
    return 0;
  for (i = 0; i < n; ++i)
    {
      unsigned char a = (unsigned char)s[i];
      if (!((a >= 'a' && a <= 'z') || (a >= 'A' && a <= 'Z')
            || (a >= '0' && a <= '9') || a == '_' || a == '-' || a == '.'))
        return 0;
    }
  memset (&t, 0, sizeof (t));
  memcpy (t.id, s, n + 1);
  t.state = MULIB_OCICONTAINER_CREATED;
  *c = t;
  return 1;
}
int
mulib_ocicontainer_can_transition (const mulib_ocicontainer_t *c, int next)
{
  if (!c)
    return 0;
  return (c->state == MULIB_OCICONTAINER_CREATED
          && (next == MULIB_OCICONTAINER_RUNNING
              || next == MULIB_OCICONTAINER_DELETED))
         || (c->state == MULIB_OCICONTAINER_RUNNING
             && next == MULIB_OCICONTAINER_STOPPED)
         || (c->state == MULIB_OCICONTAINER_STOPPED
             && next == MULIB_OCICONTAINER_DELETED);
}
int
mulib_ocicontainer_transition (mulib_ocicontainer_t *c, int next, int code)
{
  if (!mulib_ocicontainer_can_transition (c, next)
      || (next == MULIB_OCICONTAINER_STOPPED && (code < 0 || code > 255)))
    return 0;
  c->state = next;
  if (next == MULIB_OCICONTAINER_STOPPED)
    c->exit_code = code;
  return 1;
}
#endif
