/** @file mulib-sshserver.h
 * @brief SSH-2 password user-authentication payload parsing and attempt
 * policy. Define MULIB_IMPL_SSHSERVER and MULIB_IMPL_ENDIAN once. Inputs must
 * come from an authenticated/encrypted SSH transport supplied by the
 * application.
 */
#ifndef MULIB_SSHSERVER_H
#define MULIB_SSHSERVER_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_sshserver_auth
  {
    const unsigned char *user, *password;
    size_t user_length, password_length;
    int none;
  } mulib_sshserver_auth_t;
  typedef struct mulib_sshserver
  {
    size_t attempts, limit;
    int authenticated;
  } mulib_sshserver_t;
  /** @brief Parse an entire SSH_MSG_USERAUTH_REQUEST (50), service
   * ssh-connection, method none/password, password change unsupported. 1
   * success, 0 malformed, -1 unsupported. Borrowed binary-safe strings;
   * username 1..1024 and password
   * <=4096 bytes. Public-key methods require another backend. */
  int mulib_sshserver_auth_parse (const void *, size_t,
                                  mulib_sshserver_auth_t *);
  /** @brief Initialize attempt limit, 1 success/0 invalid. */
  int mulib_sshserver_init (mulib_sshserver_t *, size_t max_attempts);
  /** @brief Check one request with verifier (1 accepts). Every request
   * consumes an attempt. 1 authenticated, 0 rejected, -1
   * invalid/exhausted/already done. none is always rejected. No password
   * pointer is retained. */
  int mulib_sshserver_authenticate (
      mulib_sshserver_t *, const void *, size_t,
      int (*verify) (void *, const mulib_sshserver_auth_t *), void *);
  /** @brief Encode success (52) or failure (51, password, partial=false).
   * 1 success/0 insufficient output; used reset on failure. */
  int mulib_sshserver_auth_reply (int accepted, void *, size_t, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SSHSERVER) && !defined(MULIB_SSHSERVER_IMPLEMENTED)
#define MULIB_SSHSERVER_IMPLEMENTED
#include <string.h>
static int
mulib_sshserver_string_ (const unsigned char *p, size_t n, size_t *pos,
                         const unsigned char **v, size_t *len)
{
  size_t k;
  if (n - *pos < 4)
    return 0;
  k = mulib_endian_load_be32 (p + *pos);
  *pos += 4;
  if (k > n - *pos)
    return 0;
  *v = p + *pos;
  *len = k;
  *pos += k;
  return 1;
}
int
mulib_sshserver_auth_parse (const void *data, size_t n,
                            mulib_sshserver_auth_t *out)
{
  const unsigned char *p = data, *v;
  size_t pos = 1, len;
  mulib_sshserver_auth_t a;
  if (!p || !out || !n || p[0] != 50)
    return 0;
  memset (&a, 0, sizeof (a));
  if (!mulib_sshserver_string_ (p, n, &pos, &a.user, &a.user_length)
      || !a.user_length || a.user_length > 1024
      || !mulib_sshserver_string_ (p, n, &pos, &v, &len))
    return 0;
  if (len != 14 || memcmp (v, "ssh-connection", 14))
    return -1;
  if (!mulib_sshserver_string_ (p, n, &pos, &v, &len))
    return 0;
  if (len == 4 && !memcmp (v, "none", 4))
    a.none = 1;
  else if (len == 8 && !memcmp (v, "password", 8))
    {
      if (pos == n)
        return 0;
      if (p[pos++])
        return -1;
      if (!mulib_sshserver_string_ (p, n, &pos, &a.password,
                                    &a.password_length)
          || a.password_length > 4096)
        return 0;
    }
  else
    return -1;
  if (pos != n)
    return 0;
  *out = a;
  return 1;
}
int
mulib_sshserver_init (mulib_sshserver_t *s, size_t limit)
{
  if (!s || !limit)
    return 0;
  s->attempts = 0;
  s->limit = limit;
  s->authenticated = 0;
  return 1;
}
int
mulib_sshserver_authenticate (mulib_sshserver_t *s, const void *p, size_t n,
                              int (*verify) (void *,
                                             const mulib_sshserver_auth_t *),
                              void *ctx)
{
  mulib_sshserver_auth_t a;
  if (!s || !verify || s->authenticated || s->attempts >= s->limit)
    return -1;
  ++s->attempts;
  if (mulib_sshserver_auth_parse (p, n, &a) != 1 || a.none)
    return 0;
  if (verify (ctx, &a) != 1)
    return 0;
  s->authenticated = 1;
  return 1;
}
int
mulib_sshserver_auth_reply (int accepted, void *data, size_t cap, size_t *used)
{
  unsigned char *p = data;
  if (used)
    *used = 0;
  if (!p || !used || (accepted != 0 && accepted != 1)
      || cap < (accepted ? 1u : 14u))
    return 0;
  p[0] = accepted ? 52 : 51;
  if (!accepted)
    {
      mulib_endian_store_be32 (p + 1, 8);
      memcpy (p + 5, "password", 8);
      p[13] = 0;
    }
  *used = accepted ? 1 : 14;
  return 1;
}
#endif
