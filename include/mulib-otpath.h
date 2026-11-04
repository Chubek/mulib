/** @file mulib-otpath.h
 * @brief Alternate-spelling facade for HOTP/TOTP authentication.
 * Define MULIB_IMPL_OTPATH, OTPAUTH, AUTHCRYPTO and SECURITY. */
#ifndef MULIB_OTPATH_H
#define MULIB_OTPATH_H
#include "mulib-otpauth.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Generate a TOTP using the canonical otpauth configuration. */
int mulib_otpath_totp (const mulib_otpauth_config_t *config, uint64_t now,
                       char *out, size_t capacity);
/** Verify and consume a TOTP with persistent replay protection. */
int mulib_otpath_totp_verify (const mulib_otpauth_config_t *config,
                              const char *code, uint64_t now,
                              unsigned past, unsigned future,
                              mulib_otpauth_replay_t *replay);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OTPATH) && !defined(MULIB_OTPATH_IMPLEMENTED)
#define MULIB_OTPATH_IMPLEMENTED
int
mulib_otpath_totp (const mulib_otpauth_config_t *c, uint64_t now, char *out, size_t cap)
{
  return mulib_otpauth_totp(c,now,out,cap);
}
int
mulib_otpath_totp_verify (const mulib_otpauth_config_t *c, const char *code,
                          uint64_t now, unsigned past, unsigned future,
                          mulib_otpauth_replay_t *replay)
{
  return mulib_otpauth_totp_verify(c,code,now,past,future,replay);
}
#endif
