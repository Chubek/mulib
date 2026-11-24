/** @file mulib-otpauth.h
 * @brief RFC 4226 HOTP and RFC 6238 TOTP with bounded, replay-aware verification.
 * Define MULIB_IMPL_OTPAUTH, AUTHCRYPTO and SECURITY. Secrets are borrowed binary
 * bytes, at least 16 bytes. No secret generation, clock, storage or rate limiter. */
#ifndef MULIB_OTPAUTH_H
#define MULIB_OTPAUTH_H
#include "mulib-authcrypto.h"
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_otpauth_config {
  const void *secret;
  size_t secret_length;
  mulib_authcrypto_algorithm algorithm;
  unsigned digits;                 /**< 6, 7 or 8. */
  uint64_t period, epoch;           /**< TOTP seconds; period must be nonzero. */
} mulib_otpauth_config_t;
typedef struct mulib_otpauth_replay {
  uint64_t counter;
  int initialized;                 /**< Initialize the whole struct to zero. */
} mulib_otpauth_replay_t;
/** Strict RFC 4648 Base32 secret decoding. Case-insensitive, padded or unpadded;
 * no whitespace/separators, noncanonical tail bits or empty input. Output and
 * length unchanged on failure. Output must not overlap source. */
int mulib_otpauth_base32 (const char *source, size_t length,
                          unsigned char *out, size_t capacity, size_t *written);
/** Generate decimal code, NUL terminated. Output unchanged on failure. */
int mulib_otpauth_hotp (const mulib_otpauth_config_t *config, uint64_t counter,
                        char *out, size_t capacity);
/** Search [*next, *next+lookahead], at most 100 ahead, without wrapping. Advance
 * next past the first match only on success. UINT64_MAX cannot be consumed. */
int mulib_otpauth_hotp_verify (const mulib_otpauth_config_t *config,
                               const char *code, unsigned lookahead,
                               uint64_t *next);
/** TOTP takes explicit Unix seconds; times before epoch fail. */
int mulib_otpauth_totp (const mulib_otpauth_config_t *config, uint64_t now,
                        char *out, size_t capacity);
/** Search at most 10 steps each side. Replay state is mandatory and updated only
 * on success; older/equal counters are denied. Caller MUST serialize and persist
 * updates per credential (also across processes). No internal synchronization. */
int mulib_otpauth_totp_verify (const mulib_otpauth_config_t *config,
                               const char *code, uint64_t now,
                               unsigned past, unsigned future,
                               mulib_otpauth_replay_t *replay);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OTPAUTH) && !defined(MULIB_OTPAUTH_IMPLEMENTED)
#define MULIB_OTPAUTH_IMPLEMENTED
#include <string.h>
static int
mulib_otpauth_b32_ (unsigned c)
{
  if (c>='a' && c<='z') c-=32;
  if (c>='A' && c<='Z') return (int)(c-'A');
  return c>='2' && c<='7' ? (int)(c-'2'+26) : -1;
}
int
mulib_otpauth_base32 (const char *s, size_t n, unsigned char *out,
                       size_t cap, size_t *written)
{
  size_t data=n, need, pos=0;
  unsigned bits=0, acc=0;
  if (!s || !out || !n) return 0;
  while (data && s[data-1]=='=') --data;
  if (!data || (n!=data && (n%8 || n-data>6))) return 0;
  switch (data%8) {
    case 0: case 2: case 4: case 5: case 7: break;
    default: return 0;
  }
  need=data/8*5+(data%8)*5/8;
  if (need>cap) return 0;
  for (size_t i=0; i<data; ++i) {
    int v=mulib_otpauth_b32_ ((unsigned char)s[i]);
    if (v<0) return 0;
    acc=(acc<<5)|(unsigned)v; bits+=5;
    if (bits>=8) bits-=8;
  }
  if (bits && (acc & ((1u<<bits)-1))) return 0;
  bits=0; acc=0;
  for (size_t i=0; i<data; ++i) {
    acc=(acc<<5)|(unsigned)mulib_otpauth_b32_((unsigned char)s[i]); bits+=5;
    if (bits>=8) { bits-=8; out[pos++]=(unsigned char)(acc>>bits); }
  }
  if (written) *written=need;
  return 1;
}
static int
mulib_otpauth_valid_ (const mulib_otpauth_config_t *c)
{
  return c && c->secret && c->secret_length>=16
         && c->secret_length<=UINT64_MAX/8 && mulib_authcrypto_size(c->algorithm)
         && c->digits>=6 && c->digits<=8;
}
int
mulib_otpauth_hotp (const mulib_otpauth_config_t *c, uint64_t counter,
                     char *out, size_t cap)
{
  unsigned char bytes[8], digest[32];
  char text[9];
  uint32_t value, modulus=1;
  unsigned offset;
  if (!mulib_otpauth_valid_(c) || !out || cap<=c->digits) return 0;
  for (unsigned i=0; i<8; ++i) bytes[7-i]=(unsigned char)(counter>>(8*i));
  if (!mulib_authcrypto_hmac(c->algorithm,c->secret,c->secret_length,
                             bytes,8,digest,sizeof digest)) return 0;
  offset=digest[mulib_authcrypto_size(c->algorithm)-1]&15;
  value=((uint32_t)(digest[offset]&127)<<24)|((uint32_t)digest[offset+1]<<16)
        |((uint32_t)digest[offset+2]<<8)|digest[offset+3];
  for (unsigned i=0; i<c->digits; ++i) modulus*=10;
  value%=modulus;
  text[c->digits]=0;
  for (unsigned i=c->digits; i; --i) { text[i-1]=(char)('0'+value%10); value/=10; }
  memcpy (out,text,c->digits+1);
  mulib_authcrypto_wipe (digest,sizeof digest);
  mulib_authcrypto_wipe (text,sizeof text);
  return 1;
}
static int
mulib_otpauth_code_ (const mulib_otpauth_config_t *c, const char *code)
{
  if (!mulib_otpauth_valid_(c) || !code || strlen(code)!=c->digits) return 0;
  for (unsigned i=0; i<c->digits; ++i) if (code[i]<'0' || code[i]>'9') return 0;
  return 1;
}
int
mulib_otpauth_hotp_verify (const mulib_otpauth_config_t *c, const char *code,
                            unsigned ahead, uint64_t *next)
{
  char expected[9];
  uint64_t match=0;
  int found=0;
  if (!next || ahead>100 || !mulib_otpauth_code_(c,code)) return 0;
  for (unsigned i=0; i<=ahead; ++i) {
    uint64_t counter;
    int equal;
    if (UINT64_MAX-*next<=i) break;
    counter=*next+i;
    if (!mulib_otpauth_hotp(c,counter,expected,sizeof expected)) return 0;
    equal=mulib_security_equal(code,expected,c->digits);
    if (equal && !found) { found=1; match=counter; }
  }
  mulib_authcrypto_wipe (expected,sizeof expected);
  if (found) *next=match+1;
  return found;
}
int
mulib_otpauth_totp (const mulib_otpauth_config_t *c, uint64_t now,
                     char *out, size_t cap)
{
  if (!mulib_otpauth_valid_(c) || !c->period || now<c->epoch) return 0;
  return mulib_otpauth_hotp(c,(now-c->epoch)/c->period,out,cap);
}
int
mulib_otpauth_totp_verify (const mulib_otpauth_config_t *c, const char *code,
                            uint64_t now, unsigned past, unsigned future,
                            mulib_otpauth_replay_t *replay)
{
  uint64_t current, first, last, match=0;
  char expected[9];
  int found=0;
  if (!replay || (replay->initialized!=0 && replay->initialized!=1)
      || !mulib_otpauth_code_(c,code) || !c->period || now<c->epoch
      || past>10 || future>10) return 0;
  current=(now-c->epoch)/c->period;
  first=current<past ? 0 : current-past;
  last=UINT64_MAX-current<future ? UINT64_MAX : current+future;
  for (uint64_t counter=first;; ++counter) {
    int equal;
    if (!mulib_otpauth_hotp(c,counter,expected,sizeof expected)) return 0;
    equal=mulib_security_equal(code,expected,c->digits);
    if (equal && !found && (!replay->initialized || counter>replay->counter)) {
      found=1; match=counter;
    }
    if (counter==last) break;
  }
  mulib_authcrypto_wipe (expected,sizeof expected);
  if (found) { replay->counter=match; replay->initialized=1; }
  return found;
}
#endif
