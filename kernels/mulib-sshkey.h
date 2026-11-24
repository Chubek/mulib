/** @file mulib-sshkey.h
 * @brief ssh-ed25519 public-key and signature blobs (RFC 8709).
 * Define MULIB_IMPL_SSHKEY and SSHWIRE. Use mulib-ed25519.h for operations.
 */
#ifndef MULIB_SSHKEY_H
#define MULIB_SSHKEY_H
#include "mulib-sshwire.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SSH_ED25519_NAME "ssh-ed25519"
enum { MULIB_SSH_ED25519_KEY_BLOB = 51, MULIB_SSH_ED25519_SIG_BLOB = 83 };
int mulib_sshkey_public_write (const unsigned char key[32], void *, size_t, size_t *used);
int mulib_sshkey_signature_write (const unsigned char signature[64], void *, size_t, size_t *used);
/* Borrow the fixed-length raw value from an entire blob. Return 1/0. */
int mulib_sshkey_public_parse (const void *, size_t, mulib_ssh_slice_t *);
int mulib_sshkey_signature_parse (const void *, size_t, mulib_ssh_slice_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHKEY) && !defined(MULIB_SSHKEY_IMPLEMENTED)
#define MULIB_SSHKEY_IMPLEMENTED

static int
mulib_sshkey_write_ (const unsigned char *raw, size_t raw_length,
                      void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!raw || !out || !used || cap < 19 + raw_length
      || !mulib_ssh_write_string (&w, MULIB_SSH_ED25519_NAME, sizeof MULIB_SSH_ED25519_NAME - 1)
      || !mulib_ssh_write_string (&w, raw, raw_length)) return 0;
  *used = cap - w.left;
  return 1;
}

static int
mulib_sshkey_parse_ (const void *data, size_t n, size_t raw_length, mulib_ssh_slice_t *out)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  mulib_ssh_slice_t name, raw;
  if (!out || !mulib_ssh_read_string (&r, &name)
      || !mulib_ssh_slice_is (name, MULIB_SSH_ED25519_NAME)
      || !mulib_ssh_read_string (&r, &raw) || raw.length != raw_length || r.left) return 0;
  *out = raw;
  return 1;
}

int
mulib_sshkey_public_write (const unsigned char key[32], void *out, size_t cap, size_t *used)
{
  return mulib_sshkey_write_ (key, 32, out, cap, used);
}

int
mulib_sshkey_signature_write (const unsigned char signature[64], void *out, size_t cap, size_t *used)
{
  return mulib_sshkey_write_ (signature, 64, out, cap, used);
}

int
mulib_sshkey_public_parse (const void *data, size_t n, mulib_ssh_slice_t *out)
{
  return mulib_sshkey_parse_ (data, n, 32, out);
}

int
mulib_sshkey_signature_parse (const void *data, size_t n, mulib_ssh_slice_t *out)
{
  return mulib_sshkey_parse_ (data, n, 64, out);
}
#endif
