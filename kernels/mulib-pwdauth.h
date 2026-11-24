/** @file mulib-pwdauth.h
 * @brief Salted PBKDF2-SHA-256 password records with bounded verification cost.
 * Define MULIB_IMPL_PWDAUTH, AUTHCRYPTO and SECURITY. Application supplies a
 * unique cryptographically random 16-byte salt and protects stored records. */
#ifndef MULIB_PWDAUTH_H
#define MULIB_PWDAUTH_H
#include "mulib-authcrypto.h"
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_PWDAUTH_MIN_ITERATIONS 600000u
#define MULIB_PWDAUTH_MAX_ITERATIONS 10000000u
typedef struct mulib_pwdauth_record {
  uint32_t iterations;
  unsigned char salt[16], derived[32];
} mulib_pwdauth_record_t;
/** Create a record; work factor must be within the public bounds. Password is
 * binary, may contain NUL, and may be empty. Output unchanged on failure. */
int mulib_pwdauth_create (const void *password, size_t length,
                          const unsigned char salt[16], uint32_t iterations,
                          mulib_pwdauth_record_t *out);
/** Derive and compare without a data-dependent early exit. Invalid work factors
 * are rejected before derivation. Record must come from trusted storage. */
int mulib_pwdauth_verify (const mulib_pwdauth_record_t *record,
                          const void *password, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PWDAUTH) && !defined(MULIB_PWDAUTH_IMPLEMENTED)
#define MULIB_PWDAUTH_IMPLEMENTED
#include <string.h>
int
mulib_pwdauth_create (const void *password, size_t n, const unsigned char salt[16],
                       uint32_t iterations, mulib_pwdauth_record_t *out)
{
  mulib_pwdauth_record_t r;
  if (!salt || !out || iterations<MULIB_PWDAUTH_MIN_ITERATIONS
      || iterations>MULIB_PWDAUTH_MAX_ITERATIONS) return 0;
  memset (&r,0,sizeof r);
  r.iterations=iterations;
  memcpy (r.salt,salt,16);
  if (!mulib_authcrypto_pbkdf2(password,n,r.salt,16,iterations,r.derived)) return 0;
  *out=r;
  mulib_authcrypto_wipe (&r,sizeof r);
  return 1;
}
int
mulib_pwdauth_verify (const mulib_pwdauth_record_t *r, const void *password, size_t n)
{
  unsigned char derived[32];
  int ok;
  if (!r || r->iterations<MULIB_PWDAUTH_MIN_ITERATIONS
      || r->iterations>MULIB_PWDAUTH_MAX_ITERATIONS
      || !mulib_authcrypto_pbkdf2(password,n,r->salt,16,r->iterations,derived)) return 0;
  ok=mulib_security_equal(derived,r->derived,32);
  mulib_authcrypto_wipe (derived,sizeof derived);
  return ok;
}
#endif
