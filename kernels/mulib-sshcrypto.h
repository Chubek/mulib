/** @file mulib-sshcrypto.h
 * @brief SSH SHA-256 key derivation and directional AES-128-GCM.
 * Define MULIB_IMPL_SSHCRYPTO and AES_GCM, ENTROPY, SSHWIRE, ENDIAN. Link libcrypto.
 */
#ifndef MULIB_SSHCRYPTO_H
#define MULIB_SSHCRYPTO_H
#include "mulib-aes-gcm.h"
#include "mulib-sshpacket.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Hard limits per key/direction. Count body bytes, excluding the length
 * field and tag. The invocation counter may impose an earlier limit. */
enum { MULIB_SSH_GCM_PACKET_LIMIT = 1 << 24, MULIB_SSH_GCM_BYTE_LIMIT = 1 << 30 };

typedef struct {
  mulib_aes_gcm_t cipher;
  unsigned char nonce[12];
  uint32_t packets;
  uint64_t bytes;
  int active;
} mulib_ssh_gcm_t;

int mulib_ssh_sha256_parts (const mulib_ssh_slice_t *, size_t count, unsigned char out[32]);
/* RFC 4253 key derivation for selectors A..F, directly from the raw X25519
 * result as used by RFC 8731. Output must be disjoint from all inputs. */
int mulib_ssh_derive (const unsigned char secret[32], const unsigned char hash[32],
                      const unsigned char session_id[32], unsigned char selector,
                      void *, size_t);
int mulib_ssh_gcm_init (mulib_ssh_gcm_t *, const unsigned char key[16],
                        const unsigned char iv[12]);
void mulib_ssh_gcm_clear (mulib_ssh_gcm_t *);
/* One state per direction. Never copy an active direction for reuse.
 * Encrypt/decrypt the body in place, with the four length bytes as AAD.
 * A failed operation clears the direction. Caller must close the connection. */
int mulib_ssh_gcm_seal (mulib_ssh_gcm_t *, const unsigned char length[4],
                        void *body, size_t, unsigned char tag[16]);
int mulib_ssh_gcm_open (mulib_ssh_gcm_t *, const unsigned char length[4],
                        void *body, size_t, const unsigned char tag[16]);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHCRYPTO) && !defined(MULIB_SSHCRYPTO_IMPLEMENTED)
#define MULIB_SSHCRYPTO_IMPLEMENTED
#include <openssl/evp.h>
#include <string.h>

int
mulib_ssh_sha256_parts (const mulib_ssh_slice_t *parts, size_t count, unsigned char out[32])
{
  unsigned char digest[32];
  unsigned int length = 0;
  if ((!parts && count) || !out) return 0;
  EVP_MD_CTX *ctx = EVP_MD_CTX_new ();
  int ok = ctx && EVP_DigestInit_ex (ctx, EVP_sha256 (), NULL) == 1;
  for (size_t i = 0; ok && i < count; ++i)
    if (parts[i].length)
      ok = parts[i].data && EVP_DigestUpdate (ctx, parts[i].data, parts[i].length) == 1;
  ok = ok && EVP_DigestFinal_ex (ctx, digest, &length) == 1 && length == sizeof digest;
  EVP_MD_CTX_free (ctx);
  if (ok) memcpy (out, digest, sizeof digest);
  mulib_secret_wipe (digest, sizeof digest);
  return ok;
}

int
mulib_ssh_derive (const unsigned char secret[32], const unsigned char hash[32],
                  const unsigned char session_id[32], unsigned char selector,
                  void *out, size_t length)
{
  unsigned char mpint[37], block[32];
  mulib_ssh_writer_t w = {mpint, sizeof mpint};
  unsigned char *p = (unsigned char *)out;
  if (!secret || !hash || !session_id || selector < 'A' || selector > 'F'
      || (!p && length)) return 0;
  if (!mulib_ssh_write_mpint (&w, secret, 32)) return 0;
  mulib_ssh_slice_t parts[4] = {
    {mpint, sizeof mpint - w.left}, {hash, 32}, {&selector, 1}, {session_id, 32}
  };
  int ok = 1;
  for (size_t done = 0; done < length;) {
    if (!(ok = mulib_ssh_sha256_parts (parts, done ? 3 : 4, block))) break;
    size_t take = length - done < sizeof block ? length - done : sizeof block;
    memcpy (p + done, block, take);
    done += take;
    parts[2].data = p;
    parts[2].length = done;
  }
  if (!ok) mulib_secret_wipe (p, length);
  mulib_secret_wipe (mpint, sizeof mpint);
  mulib_secret_wipe (block, sizeof block);
  return ok;
}

int
mulib_ssh_gcm_init (mulib_ssh_gcm_t *g, const unsigned char key[16],
                    const unsigned char iv[12])
{
  if (!g || !key || !iv || !mulib_aes_gcm_init (&g->cipher, key)) return 0;
  memcpy (g->nonce, iv, 12);
  g->packets = 0;
  g->bytes = 0;
  g->active = 1;
  return 1;
}

void
mulib_ssh_gcm_clear (mulib_ssh_gcm_t *g)
{
  if (g) mulib_secret_wipe (g, sizeof *g);
}

static int
mulib_ssh_gcm_ready_ (const mulib_ssh_gcm_t *g, const unsigned char length[4], size_t n)
{
  /* The eight-byte invocation field can start near wrap. */
  return g && g->active && length && n >= 16 && n <= MULIB_SSH_BODY_MAX && n % 16 == 0
         && n == mulib_endian_load_be32 (length)
         && g->packets < MULIB_SSH_GCM_PACKET_LIMIT
         && g->bytes <= MULIB_SSH_GCM_BYTE_LIMIT - n
         && mulib_endian_load_be64 (g->nonce + 4) != UINT64_MAX;
}

static int
mulib_ssh_gcm_finish_ (mulib_ssh_gcm_t *g, size_t n, int ok)
{
  if (!ok) {
    mulib_ssh_gcm_clear (g);
    return 0;
  }
  mulib_endian_store_be64 (g->nonce + 4, mulib_endian_load_be64 (g->nonce + 4) + 1);
  ++g->packets;
  g->bytes += n;
  return 1;
}

int
mulib_ssh_gcm_seal (mulib_ssh_gcm_t *g, const unsigned char length[4],
                    void *body, size_t n, unsigned char tag[16])
{
  int ok = mulib_ssh_gcm_ready_ (g, length, n)
           && mulib_aes_gcm_seal (&g->cipher, g->nonce, length, 4, body, n, body, tag);
  return mulib_ssh_gcm_finish_ (g, n, ok);
}

int
mulib_ssh_gcm_open (mulib_ssh_gcm_t *g, const unsigned char length[4],
                    void *body, size_t n, const unsigned char tag[16])
{
  int ok = mulib_ssh_gcm_ready_ (g, length, n)
           && mulib_aes_gcm_open (&g->cipher, g->nonce, length, 4, body, n, tag, body);
  return mulib_ssh_gcm_finish_ (g, n, ok);
}
#endif
