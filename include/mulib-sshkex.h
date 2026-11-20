/** @file mulib-sshkex.h
 * @brief SSH curve25519-sha256 negotiation, ECDH payloads and exchange hash.
 * Define MULIB_IMPL_SSHKEX and SSHCRYPTO, SSHPROTO, SSHKEY, ED25519, X25519.
 */
#ifndef MULIB_SSHKEX_H
#define MULIB_SSHKEX_H
#include "mulib-ed25519.h"
#include "mulib-sshcrypto.h"
#include "mulib-sshkey.h"
#include "mulib-sshproto.h"
#include "mulib-x25519.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SSH_CURVE25519_NAME "curve25519-sha256"
#define MULIB_SSH_GCM_NAME "aes128-gcm@openssh.com"
enum { MULIB_SSH_ECDH_INIT_SIZE = 37, MULIB_SSH_ECDH_REPLY_SIZE = 179 };
typedef struct {
  mulib_ssh_slice_t client_version, server_version;
  mulib_ssh_slice_t client_kexinit, server_kexinit;
} mulib_ssh_transcript_t;

/* A fixed suite: curve25519-sha256, ssh-ed25519, aes128-gcm@openssh.com
 * and no compression. GCM supplies its own authentication in each direction. */
int mulib_sshkex_propose (const unsigned char cookie[16], void *, size_t, size_t *used);
int mulib_sshkex_negotiate (const void *, size_t, int *skip_guess);
int mulib_sshkex_init_write (const unsigned char public_key[32], void *, size_t, size_t *used);
int mulib_sshkex_init_parse (const void *, size_t, mulib_ssh_slice_t *public_key);
int mulib_sshkex_reply_write (const unsigned char host[32], const unsigned char ephemeral[32],
                             const unsigned char signature[64], void *, size_t, size_t *used);
int mulib_sshkex_reply_parse (const void *, size_t, mulib_ssh_slice_t *host,
                             mulib_ssh_slice_t *ephemeral, mulib_ssh_slice_t *signature);
/* Versions exclude CRLF. KEXINIT slices include their message byte. The host
 * argument is the raw Ed25519 public key; K is the raw X25519 result. */
int mulib_sshkex_hash (const mulib_ssh_transcript_t *, const unsigned char host[32],
                      const unsigned char client[32], const unsigned char server[32],
                      const unsigned char secret[32], unsigned char hash[32]);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHKEX) && !defined(MULIB_SSHKEX_IMPLEMENTED)
#define MULIB_SSHKEX_IMPLEMENTED
#include <string.h>

int
mulib_sshkex_propose (const unsigned char cookie[16], void *out, size_t cap, size_t *used)
{
  static const char *const names[MULIB_SSH_LIST_COUNT] = {
    MULIB_SSH_CURVE25519_NAME, MULIB_SSH_ED25519_NAME, MULIB_SSH_GCM_NAME, MULIB_SSH_GCM_NAME,
    "hmac-sha2-256", "hmac-sha2-256", "none", "none", "", ""
  };
  mulib_ssh_kexinit_t msg;
  if (used) *used = 0;
  if (!cookie) return 0;
  memcpy (msg.cookie, cookie, sizeof msg.cookie);
  msg.follows = 0;
  for (size_t i = 0; i < MULIB_SSH_LIST_COUNT; ++i) {
    msg.lists[i].data = (const unsigned char *)names[i];
    msg.lists[i].length = strlen (names[i]);
  }
  return mulib_ssh_kexinit_write (&msg, out, cap, used);
}

int
mulib_sshkex_negotiate (const void *data, size_t n, int *skip_guess)
{
  mulib_ssh_kexinit_t msg;
  if (!skip_guess || !mulib_ssh_kexinit_parse (data, n, &msg)
      || !mulib_ssh_namelist_has (msg.lists[MULIB_SSH_LIST_KEX], MULIB_SSH_CURVE25519_NAME)
      || !mulib_ssh_namelist_has (msg.lists[MULIB_SSH_LIST_HOST], MULIB_SSH_ED25519_NAME)
      || !mulib_ssh_namelist_has (msg.lists[MULIB_SSH_LIST_CIPHER_C2S], MULIB_SSH_GCM_NAME)
      || !mulib_ssh_namelist_has (msg.lists[MULIB_SSH_LIST_CIPHER_S2C], MULIB_SSH_GCM_NAME)
      || !mulib_ssh_namelist_has (msg.lists[MULIB_SSH_LIST_COMP_C2S], "none")
      || !mulib_ssh_namelist_has (msg.lists[MULIB_SSH_LIST_COMP_S2C], "none")) return 0;
  *skip_guess = msg.follows
    && (!mulib_ssh_slice_is (mulib_ssh_namelist_first (msg.lists[MULIB_SSH_LIST_KEX]),
                             MULIB_SSH_CURVE25519_NAME)
        || !mulib_ssh_slice_is (mulib_ssh_namelist_first (msg.lists[MULIB_SSH_LIST_HOST]),
                               MULIB_SSH_ED25519_NAME));
  return 1;
}

int
mulib_sshkex_init_write (const unsigned char key[32], void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!key || !used || cap < MULIB_SSH_ECDH_INIT_SIZE
      || !mulib_ssh_write_u8 (&w, MULIB_SSH_ECDH_INIT)
      || !mulib_ssh_write_string (&w, key, 32)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshkex_init_parse (const void *data, size_t n, mulib_ssh_slice_t *out)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  unsigned char type;
  mulib_ssh_slice_t key;
  if (!out || !mulib_ssh_read_u8 (&r, &type) || type != MULIB_SSH_ECDH_INIT
      || !mulib_ssh_read_string (&r, &key) || key.length != 32 || r.left) return 0;
  *out = key;
  return 1;
}

int
mulib_sshkex_reply_write (const unsigned char host[32], const unsigned char ephemeral[32],
                         const unsigned char signature[64], void *out, size_t cap, size_t *used)
{
  unsigned char key_blob[MULIB_SSH_ED25519_KEY_BLOB], sig_blob[MULIB_SSH_ED25519_SIG_BLOB];
  size_t key_size, sig_size;
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || !ephemeral || cap < MULIB_SSH_ECDH_REPLY_SIZE
      || !mulib_sshkey_public_write (host, key_blob, sizeof key_blob, &key_size)
      || !mulib_sshkey_signature_write (signature, sig_blob, sizeof sig_blob, &sig_size)
      || !mulib_ssh_write_u8 (&w, MULIB_SSH_ECDH_REPLY)
      || !mulib_ssh_write_string (&w, key_blob, key_size)
      || !mulib_ssh_write_string (&w, ephemeral, 32)
      || !mulib_ssh_write_string (&w, sig_blob, sig_size)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshkex_reply_parse (const void *data, size_t n, mulib_ssh_slice_t *host,
                         mulib_ssh_slice_t *ephemeral, mulib_ssh_slice_t *signature)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  unsigned char type;
  mulib_ssh_slice_t h, e, s, raw_host, raw_sig;
  if (!host || !ephemeral || !signature
      || !mulib_ssh_read_u8 (&r, &type) || type != MULIB_SSH_ECDH_REPLY
      || !mulib_ssh_read_string (&r, &h) || !mulib_sshkey_public_parse (h.data, h.length, &raw_host)
      || !mulib_ssh_read_string (&r, &e) || e.length != 32
      || !mulib_ssh_read_string (&r, &s) || !mulib_sshkey_signature_parse (s.data, s.length, &raw_sig)
      || r.left) return 0;
  *host = raw_host;
  *ephemeral = e;
  *signature = raw_sig;
  return 1;
}

int
mulib_sshkex_hash (const mulib_ssh_transcript_t *t, const unsigned char host[32],
                  const unsigned char client[32], const unsigned char server[32],
                  const unsigned char secret[32], unsigned char hash[32])
{
  unsigned char key_blob[MULIB_SSH_ED25519_KEY_BLOB], mpint[37], lengths[7][4];
  size_t key_length;
  mulib_ssh_slice_t parts[15];
  mulib_ssh_writer_t w = {mpint, sizeof mpint};
  if (!t || !client || !server || !secret || !hash
      || !mulib_sshkey_public_write (host, key_blob, sizeof key_blob, &key_length)) return 0;
  const mulib_ssh_slice_t values[7] = {
    t->client_version, t->server_version, t->client_kexinit, t->server_kexinit,
    {key_blob, key_length}, {client, 32}, {server, 32}
  };
  for (size_t i = 0; i < 7; ++i) {
    if (!values[i].data || !values[i].length || values[i].length > UINT32_MAX) return 0;
    mulib_endian_store_be32 (lengths[i], (uint32_t)values[i].length);
    parts[2 * i].data = lengths[i];
    parts[2 * i].length = sizeof lengths[i];
    parts[2 * i + 1] = values[i];
  }
  if (!mulib_ssh_write_mpint (&w, secret, 32)) return 0;
  parts[14].data = mpint;
  parts[14].length = sizeof mpint - w.left;
  int ok = mulib_ssh_sha256_parts (parts, 15, hash);
  mulib_secret_wipe (mpint, sizeof mpint);
  return ok;
}
#endif
