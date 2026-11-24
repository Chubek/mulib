/** @file mulib-vcscommit.h
 * @brief Canonical Git commit construction and repository insertion.
 * Define MULIB_IMPL_VCSCOMMIT plus GIT, VCSREPO and STRBUF implementations.
 */
#ifndef MULIB_VCSCOMMIT_H
#define MULIB_VCSCOMMIT_H
#include "mulib-strbuf.h"
#include "mulib-vcsrepo.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vcscommit_identity
  {
    const char *name, *email;
    int64_t timestamp;
    int timezone_minutes;
  } mulib_vcscommit_identity_t;
  typedef struct mulib_vcscommit
  {
    mulib_git_oid_t tree;
    const mulib_git_oid_t *parents;
    size_t parent_count;
    mulib_vcscommit_identity_t author, committer;
    const void *message;
    size_t message_length;
  } mulib_vcscommit_t;
  /** @brief Serialize to malloc-owned NUL-terminated bytes (NUL excluded from
   * length). <=64 parents, <=16 MiB message, identity fields <=1024 bytes.
   * No signatures/extra headers. 1 success/0 failure; outputs unchanged on
   * failure. Inputs are borrowed. Output must not already own storage. */
  int mulib_vcscommit_encode (const mulib_vcscommit_t *, char **bytes,
                              size_t *length);
  /** @brief Validate referenced tree/parent object types then insert commit.
   * 1 success/0 invalid, missing dependency, limit or allocation error. */
  int mulib_vcscommit_store (mulib_vcsrepo_t *, const mulib_vcscommit_t *,
                             mulib_git_oid_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VCSCOMMIT) && !defined(MULIB_VCSCOMMIT_IMPLEMENTED)
#define MULIB_VCSCOMMIT_IMPLEMENTED
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int
mulib_vcscommit_field_ (const char *s)
{
  size_t n;
  if (!s || !*s)
    return 0;
  n = strlen (s);
  if (n > 1024 || s[0] == ' ' || s[n - 1] == ' ')
    return 0;
  for (size_t i = 0; i < n; ++i)
    if ((unsigned char)s[i] < 32 || s[i] == 127 || s[i] == '<' || s[i] == '>')
      return 0;
  return 1;
}
static int
mulib_vcscommit_identity_ (mulib_strbuf_t *s, const char *role,
                           const mulib_vcscommit_identity_t *id)
{
  char time[96];
  int z = id->timezone_minutes, k;
  if (!mulib_vcscommit_field_ (id->name) || !mulib_vcscommit_field_ (id->email)
      || z < -1439 || z > 1439)
    return 0;
  k = snprintf (time, sizeof (time), "> %" PRId64 " %c%02d%02d\n",
                id->timestamp, z < 0 ? '-' : '+', (z < 0 ? -z : z) / 60,
                (z < 0 ? -z : z) % 60);
  return k > 0 && (size_t)k < sizeof (time) && mulib_strbuf_append (s, role)
         && mulib_strbuf_append (s, id->name) && mulib_strbuf_append (s, " <")
         && mulib_strbuf_append (s, id->email)
         && mulib_strbuf_append (s, time);
}
int
mulib_vcscommit_encode (const mulib_vcscommit_t *c, char **bytes,
                        size_t *length)
{
  mulib_strbuf_t s;
  char hex[41];
  int ok;
  if (!c || !bytes || !length || (!c->parents && c->parent_count)
      || c->parent_count > 64 || (!c->message && c->message_length)
      || c->message_length > 16777216)
    return 0;
  mulib_strbuf_init (&s);
  mulib_git_oid_format (&c->tree, hex);
  ok = mulib_strbuf_append (&s, "tree ") && mulib_strbuf_append (&s, hex)
       && mulib_strbuf_putc (&s, '\n');
  for (size_t i = 0; ok && i < c->parent_count; ++i)
    {
      mulib_git_oid_format (c->parents + i, hex);
      ok = mulib_strbuf_append (&s, "parent ") && mulib_strbuf_append (&s, hex)
           && mulib_strbuf_putc (&s, '\n');
    }
  ok = ok && mulib_vcscommit_identity_ (&s, "author ", &c->author)
       && mulib_vcscommit_identity_ (&s, "committer ", &c->committer)
       && mulib_strbuf_putc (&s, '\n')
       && mulib_strbuf_append_n (&s, c->message, c->message_length);
  if (!ok)
    {
      mulib_strbuf_free (&s);
      return 0;
    }
  *bytes = s.data;
  *length = s.length;
  return 1;
}
int
mulib_vcscommit_store (mulib_vcsrepo_t *r, const mulib_vcscommit_t *c,
                       mulib_git_oid_t *id)
{
  const mulib_vcsrepo_object_t *o;
  char *p;
  size_t n;
  int result;
  if (!r || !c || !id || (!c->parents && c->parent_count)
      || c->parent_count > 64)
    return 0;
  o = mulib_vcsrepo_get (r, &c->tree);
  if (!o || strcmp (o->type, "tree"))
    return 0;
  for (size_t i = 0; i < c->parent_count; ++i)
    {
      o = mulib_vcsrepo_get (r, c->parents + i);
      if (!o || strcmp (o->type, "commit"))
        return 0;
    }
  if (!mulib_vcscommit_encode (c, &p, &n))
    return 0;
  result = mulib_vcsrepo_put (r, "commit", p, n, id);
  free (p);
  return result == 1;
}
#endif
