/** @file mulib-sshauth.h
 * @brief SSH user-authentication payloads for none, password and ssh-ed25519.
 * Define MULIB_IMPL_SSHAUTH and SSHPROTO, SSHKEY, SSHWIRE, ED25519.
 */
#ifndef MULIB_SSHAUTH_H
#define MULIB_SSHAUTH_H
#include "mulib-ed25519.h"
#include "mulib-sshkey.h"
#include "mulib-sshproto.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_SSH_USER_MAX = 1024, MULIB_SSH_PASSWORD_MAX = 4096 };
typedef enum {
  MULIB_SSH_AUTH_NONE = 1, MULIB_SSH_AUTH_PASSWORD, MULIB_SSH_AUTH_ED25519
} mulib_ssh_auth_method_t;

typedef struct {
  mulib_ssh_auth_method_t method;
  mulib_ssh_slice_t user, password, public_key, signature;
} mulib_ssh_auth_request_t;

typedef struct {
  unsigned char type;
  int partial;
  mulib_ssh_slice_t methods, message, language;
} mulib_ssh_auth_reply_t;

/* Use these payloads only over an authenticated encrypted transport. Encoders
 * return 1/0 with used=0 on failure and require disjoint buffers. */
int mulib_sshauth_service_write (int accept, void *, size_t, size_t *used);
int mulib_sshauth_service_parse (int accept, const void *, size_t);
int mulib_sshauth_none_write (mulib_ssh_slice_t user, void *, size_t, size_t *used);
int mulib_sshauth_password_write (mulib_ssh_slice_t user, mulib_ssh_slice_t password,
                                  void *, size_t, size_t *used);
int mulib_sshauth_public_query (mulib_ssh_slice_t user, const unsigned char key[32],
                                void *, size_t, size_t *used);
int mulib_sshauth_public_write (mulib_ssh_slice_t user, const unsigned char seed[32],
                                const unsigned char session_id[32], void *, size_t, size_t *used);
/* Return 1 supported, 0 malformed, -1 unsupported. Pointers borrow the input.
 * An unsigned public-key query has an empty signature. */
int mulib_sshauth_parse (const void *, size_t, mulib_ssh_auth_request_t *);
/* Parse and verify a signed Ed25519 request against the transport session ID.
 * A successful signature proves possession. The application authorizes the
 * public key for the requested account separately. Output borrows input. */
int mulib_sshauth_public_verify (const void *, size_t, const unsigned char session_id[32],
                                 mulib_ssh_auth_request_t *);
int mulib_sshauth_pk_ok_write (const unsigned char key[32], void *, size_t, size_t *used);
int mulib_sshauth_pk_ok_parse (const void *, size_t, mulib_ssh_slice_t *key);
int mulib_sshauth_reply_write (int accepted, mulib_ssh_slice_t methods, int partial,
                               void *, size_t, size_t *used);
int mulib_sshauth_banner_write (mulib_ssh_slice_t message, mulib_ssh_slice_t language,
                                void *, size_t, size_t *used);
/* Decode SUCCESS, FAILURE, or BANNER. Result borrows the entire input. */
int mulib_sshauth_reply_parse (const void *, size_t, mulib_ssh_auth_reply_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHAUTH) && !defined(MULIB_SSHAUTH_IMPLEMENTED)
#define MULIB_SSHAUTH_IMPLEMENTED
#include <string.h>

enum { MULIB_SSH_AUTH_PREFIX_MAX_ = MULIB_SSH_USER_MAX + 107 };

static int
mulib_sshauth_request_prefix_ (mulib_ssh_writer_t *w, mulib_ssh_slice_t user,
                                const char *method)
{
  return user.length && user.length <= MULIB_SSH_USER_MAX
    && mulib_ssh_write_u8 (w, MULIB_SSH_USERAUTH_REQUEST)
    && mulib_ssh_write_string (w, user.data, user.length)
    && mulib_ssh_write_string (w, "ssh-connection", 14)
    && mulib_ssh_write_string (w, method, strlen (method));
}

static int
mulib_sshauth_key_write_ (mulib_ssh_writer_t *w, const unsigned char key[32])
{
  unsigned char blob[MULIB_SSH_ED25519_KEY_BLOB];
  size_t n;
  return mulib_sshkey_public_write (key, blob, sizeof blob, &n)
    && mulib_ssh_write_string (w, MULIB_SSH_ED25519_NAME, sizeof MULIB_SSH_ED25519_NAME - 1)
    && mulib_ssh_write_string (w, blob, n);
}

static int
mulib_sshauth_key_parse_ (mulib_ssh_reader_t *r, mulib_ssh_slice_t *key)
{
  mulib_ssh_slice_t algorithm, blob;
  if (!mulib_ssh_read_string (r, &algorithm) || !mulib_ssh_read_string (r, &blob)) return 0;
  if (!mulib_ssh_slice_is (algorithm, MULIB_SSH_ED25519_NAME)) return -1;
  return mulib_sshkey_public_parse (blob.data, blob.length, key);
}

int
mulib_sshauth_service_write (int accept, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || (accept != 0 && accept != 1)
      || !mulib_ssh_write_u8 (&w, accept ? MULIB_SSH_SERVICE_ACCEPT : MULIB_SSH_SERVICE_REQUEST)
      || !mulib_ssh_write_string (&w, "ssh-userauth", 12)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshauth_service_parse (int accept, const void *data, size_t n)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  unsigned char type;
  mulib_ssh_slice_t v;
  return (accept == 0 || accept == 1) && mulib_ssh_read_u8 (&r, &type)
    && type == (accept ? MULIB_SSH_SERVICE_ACCEPT : MULIB_SSH_SERVICE_REQUEST)
    && mulib_ssh_read_string (&r, &v) && !r.left && mulib_ssh_slice_is (v, "ssh-userauth");
}

int
mulib_sshauth_none_write (mulib_ssh_slice_t user, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || !mulib_sshauth_request_prefix_ (&w, user, "none")) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshauth_password_write (mulib_ssh_slice_t user, mulib_ssh_slice_t password,
                              void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  /* Check total capacity before copying any password bytes. */
  if (!used || user.length > MULIB_SSH_USER_MAX || password.length > MULIB_SSH_PASSWORD_MAX
      || cap < 40 + user.length + password.length
      || !mulib_sshauth_request_prefix_ (&w, user, "password")
      || !mulib_ssh_write_u8 (&w, 0)
      || !mulib_ssh_write_string (&w, password.data, password.length)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshauth_public_query (mulib_ssh_slice_t user, const unsigned char key[32],
                            void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || !mulib_sshauth_request_prefix_ (&w, user, "publickey")
      || !mulib_ssh_write_u8 (&w, 0) || !mulib_sshauth_key_write_ (&w, key)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshauth_public_write (mulib_ssh_slice_t user, const unsigned char seed[32],
                            const unsigned char session_id[32], void *out, size_t cap, size_t *used)
{
  unsigned char signed_data[36 + MULIB_SSH_AUTH_PREFIX_MAX_], key[32], signature[64];
  unsigned char blob[MULIB_SSH_ED25519_SIG_BLOB];
  mulib_ssh_writer_t s = {signed_data, sizeof signed_data}, w = {(unsigned char *)out, cap};
  size_t blob_size;
  if (used) *used = 0;
  if (!used || !mulib_ssh_write_string (&s, session_id, 32)
      || !mulib_ed25519_public (key, seed)
      || !mulib_sshauth_request_prefix_ (&s, user, "publickey")
      || !mulib_ssh_write_u8 (&s, 1) || !mulib_sshauth_key_write_ (&s, key)
      || !mulib_ed25519_sign (signature, seed, signed_data, sizeof signed_data - s.left)
      || !mulib_sshkey_signature_write (signature, blob, sizeof blob, &blob_size)
      || !mulib_ssh_write_bytes (&w, signed_data + 36, sizeof signed_data - s.left - 36)
      || !mulib_ssh_write_string (&w, blob, blob_size)) return 0;
  *used = cap - w.left;
  return 1;
}

static int
mulib_sshauth_parse_ (const void *data, size_t n, mulib_ssh_auth_request_t *out,
                       size_t *prefix)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  mulib_ssh_auth_request_t request;
  memset (&request, 0, sizeof request);
  unsigned char type, flag;
  mulib_ssh_slice_t service, method, blob;
  if (!out || !mulib_ssh_read_u8 (&r, &type) || type != MULIB_SSH_USERAUTH_REQUEST
      || !mulib_ssh_read_string (&r, &request.user)
      || !request.user.length || request.user.length > MULIB_SSH_USER_MAX
      || !mulib_ssh_read_string (&r, &service) || !mulib_ssh_read_string (&r, &method)) return 0;
  if (!mulib_ssh_slice_is (service, "ssh-connection")) return -1;
  if (mulib_ssh_slice_is (method, "none")) {
    request.method = MULIB_SSH_AUTH_NONE;
  } else if (mulib_ssh_slice_is (method, "password")) {
    request.method = MULIB_SSH_AUTH_PASSWORD;
    if (!mulib_ssh_read_u8 (&r, &flag)) return 0;
    if (flag) return -1;
    if (!mulib_ssh_read_string (&r, &request.password)
        || request.password.length > MULIB_SSH_PASSWORD_MAX) return 0;
  } else if (mulib_ssh_slice_is (method, "publickey")) {
    request.method = MULIB_SSH_AUTH_ED25519;
    if (!mulib_ssh_read_u8 (&r, &flag)) return 0;
    int status = mulib_sshauth_key_parse_ (&r, &request.public_key);
    if (status != 1) return status;
    *prefix = n - r.left;
    if (flag && (!mulib_ssh_read_string (&r, &blob)
                 || !mulib_sshkey_signature_parse (blob.data, blob.length, &request.signature))) return 0;
  } else {
    return -1;
  }
  if (r.left) return 0;
  *out = request;
  return 1;
}

int
mulib_sshauth_parse (const void *data, size_t n, mulib_ssh_auth_request_t *out)
{
  size_t prefix;
  return mulib_sshauth_parse_ (data, n, out, &prefix);
}

int
mulib_sshauth_public_verify (const void *data, size_t n, const unsigned char session_id[32],
                             mulib_ssh_auth_request_t *out)
{
  unsigned char signed_data[36 + MULIB_SSH_AUTH_PREFIX_MAX_];
  mulib_ssh_writer_t w = {signed_data, sizeof signed_data};
  mulib_ssh_auth_request_t request;
  size_t prefix = 0;
  if (!out || mulib_sshauth_parse_ (data, n, &request, &prefix) != 1
      || request.method != MULIB_SSH_AUTH_ED25519 || !request.signature.length
      || !mulib_ssh_write_string (&w, session_id, 32)
      || !mulib_ssh_write_bytes (&w, data, prefix)
      || !mulib_ed25519_verify (request.signature.data, request.public_key.data,
                                signed_data, sizeof signed_data - w.left)) return 0;
  *out = request;
  return 1;
}

int
mulib_sshauth_pk_ok_write (const unsigned char key[32], void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || !mulib_ssh_write_u8 (&w, MULIB_SSH_USERAUTH_PK_OK)
      || !mulib_sshauth_key_write_ (&w, key)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshauth_pk_ok_parse (const void *data, size_t n, mulib_ssh_slice_t *key)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  mulib_ssh_slice_t raw;
  unsigned char type;
  if (!key || !mulib_ssh_read_u8 (&r, &type) || type != MULIB_SSH_USERAUTH_PK_OK
      || mulib_sshauth_key_parse_ (&r, &raw) != 1 || r.left) return 0;
  *key = raw;
  return 1;
}

int
mulib_sshauth_reply_write (int accepted, mulib_ssh_slice_t methods, int partial,
                           void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || (accepted != 0 && accepted != 1) || (partial != 0 && partial != 1)
      || (accepted && partial) || (!accepted && !mulib_ssh_namelist_valid (methods))
      || !mulib_ssh_write_u8 (&w, accepted ? MULIB_SSH_USERAUTH_SUCCESS : MULIB_SSH_USERAUTH_FAILURE)) return 0;
  if (!accepted && (!mulib_ssh_write_string (&w, methods.data, methods.length)
                    || !mulib_ssh_write_u8 (&w, (unsigned char)partial))) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshauth_banner_write (mulib_ssh_slice_t message, mulib_ssh_slice_t language,
                            void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!used || !mulib_ssh_write_u8 (&w, MULIB_SSH_USERAUTH_BANNER)
      || !mulib_ssh_write_string (&w, message.data, message.length)
      || !mulib_ssh_write_string (&w, language.data, language.length)) return 0;
  *used = cap - w.left;
  return 1;
}

int
mulib_sshauth_reply_parse (const void *data, size_t n, mulib_ssh_auth_reply_t *out)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, n};
  mulib_ssh_auth_reply_t reply;
  memset (&reply, 0, sizeof reply);
  unsigned char partial;
  if (!out || !mulib_ssh_read_u8 (&r, &reply.type)) return 0;
  switch (reply.type) {
    case MULIB_SSH_USERAUTH_SUCCESS:
      break;
    case MULIB_SSH_USERAUTH_FAILURE:
      if (!mulib_ssh_read_string (&r, &reply.methods) || !mulib_ssh_namelist_valid (reply.methods)
          || !mulib_ssh_read_u8 (&r, &partial)) return 0;
      reply.partial = partial != 0;
      break;
    case MULIB_SSH_USERAUTH_BANNER:
      if (!mulib_ssh_read_string (&r, &reply.message) || !mulib_ssh_read_string (&r, &reply.language)) return 0;
      break;
    default:
      return 0;
  }
  if (r.left) return 0;
  *out = reply;
  return 1;
}
#endif
