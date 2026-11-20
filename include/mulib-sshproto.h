/** @file mulib-sshproto.h
 * @brief SSH-2 message numbers and KEXINIT payloads. Define MULIB_IMPL_SSHPROTO.
 */
#ifndef MULIB_SSHPROTO_H
#define MULIB_SSHPROTO_H
#include "mulib-sshwire.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_SSH_DISCONNECT = 1, MULIB_SSH_IGNORE = 2, MULIB_SSH_UNIMPLEMENTED = 3,
  MULIB_SSH_DEBUG = 4, MULIB_SSH_SERVICE_REQUEST = 5, MULIB_SSH_SERVICE_ACCEPT = 6,
  MULIB_SSH_KEXINIT = 20, MULIB_SSH_NEWKEYS = 21, MULIB_SSH_ECDH_INIT = 30,
  MULIB_SSH_ECDH_REPLY = 31, MULIB_SSH_USERAUTH_REQUEST = 50,
  MULIB_SSH_USERAUTH_FAILURE = 51, MULIB_SSH_USERAUTH_SUCCESS = 52,
  MULIB_SSH_USERAUTH_BANNER = 53, MULIB_SSH_USERAUTH_PK_OK = 60,
  MULIB_SSH_CHANNEL_OPEN = 90, MULIB_SSH_CHANNEL_OPEN_CONFIRMATION = 91,
  MULIB_SSH_CHANNEL_OPEN_FAILURE = 92, MULIB_SSH_CHANNEL_WINDOW_ADJUST = 93,
  MULIB_SSH_CHANNEL_DATA = 94, MULIB_SSH_CHANNEL_EXTENDED_DATA = 95,
  MULIB_SSH_CHANNEL_EOF = 96, MULIB_SSH_CHANNEL_CLOSE = 97,
  MULIB_SSH_CHANNEL_REQUEST = 98, MULIB_SSH_CHANNEL_SUCCESS = 99,
  MULIB_SSH_CHANNEL_FAILURE = 100
};

enum {
  MULIB_SSH_LIST_KEX, MULIB_SSH_LIST_HOST,
  MULIB_SSH_LIST_CIPHER_C2S, MULIB_SSH_LIST_CIPHER_S2C,
  MULIB_SSH_LIST_MAC_C2S, MULIB_SSH_LIST_MAC_S2C,
  MULIB_SSH_LIST_COMP_C2S, MULIB_SSH_LIST_COMP_S2C,
  MULIB_SSH_LIST_LANG_C2S, MULIB_SSH_LIST_LANG_S2C, MULIB_SSH_LIST_COUNT
};

typedef struct {
  unsigned char cookie[16];
  mulib_ssh_slice_t lists[MULIB_SSH_LIST_COUNT];
  int follows;
} mulib_ssh_kexinit_t;

/* Borrow name lists from the entire input payload. Encode with disjoint
 * input and output. Return 1/0, with used=0 on encode failure. */
int mulib_ssh_kexinit_parse (const void *, size_t, mulib_ssh_kexinit_t *);
int mulib_ssh_kexinit_write (const mulib_ssh_kexinit_t *, void *, size_t, size_t *used);
int mulib_ssh_namelist_valid (mulib_ssh_slice_t);
int mulib_ssh_namelist_has (mulib_ssh_slice_t, const char *name);
mulib_ssh_slice_t mulib_ssh_namelist_first (mulib_ssh_slice_t);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHPROTO) && !defined(MULIB_SSHPROTO_IMPLEMENTED)
#define MULIB_SSHPROTO_IMPLEMENTED
#include <string.h>

int
mulib_ssh_namelist_valid (mulib_ssh_slice_t list)
{
  size_t name_length = 0;
  if (!list.length) return 1;
  if (!list.data) return 0;
  for (size_t i = 0; i < list.length; ++i) {
    unsigned char c = list.data[i];
    if (c == ',') {
      if (!name_length) return 0;
      name_length = 0;
    } else if (c < 33 || c > 126 || ++name_length > 64) {
      return 0;
    }
  }
  return name_length != 0;
}

mulib_ssh_slice_t
mulib_ssh_namelist_first (mulib_ssh_slice_t list)
{
  const unsigned char *end = list.data && list.length
                             ? (const unsigned char *)memchr (list.data, ',', list.length)
                             : NULL;
  if (end) list.length = (size_t)(end - list.data);
  return list;
}

int
mulib_ssh_namelist_has (mulib_ssh_slice_t list, const char *name)
{
  if (!name || !*name || !list.data) return 0;
  while (list.length) {
    mulib_ssh_slice_t item = mulib_ssh_namelist_first (list);
    if (mulib_ssh_slice_is (item, name)) return 1;
    if (item.length == list.length) break;
    list.data += item.length + 1;
    list.length -= item.length + 1;
  }
  return 0;
}

int
mulib_ssh_kexinit_parse (const void *data, size_t size, mulib_ssh_kexinit_t *out)
{
  mulib_ssh_reader_t r = {(const unsigned char *)data, size};
  mulib_ssh_kexinit_t msg;
  unsigned char type, follows;
  mulib_ssh_slice_t cookie;
  uint32_t reserved;
  if (!out || !mulib_ssh_read_u8 (&r, &type) || type != MULIB_SSH_KEXINIT
      || !mulib_ssh_read_bytes (&r, sizeof msg.cookie, &cookie)) return 0;
  memcpy (msg.cookie, cookie.data, sizeof msg.cookie);
  for (size_t i = 0; i < MULIB_SSH_LIST_COUNT; ++i)
    if (!mulib_ssh_read_string (&r, &msg.lists[i])
        || !mulib_ssh_namelist_valid (msg.lists[i])) return 0;
  if (!mulib_ssh_read_u8 (&r, &follows) || !mulib_ssh_read_u32 (&r, &reserved)
      || reserved || r.left) return 0;
  msg.follows = follows != 0;
  *out = msg;
  return 1;
}

int
mulib_ssh_kexinit_write (const mulib_ssh_kexinit_t *msg, void *out, size_t cap, size_t *used)
{
  mulib_ssh_writer_t w = {(unsigned char *)out, cap};
  if (used) *used = 0;
  if (!msg || !out || !used || (msg->follows != 0 && msg->follows != 1)) return 0;
  for (size_t i = 0; i < MULIB_SSH_LIST_COUNT; ++i)
    if (!mulib_ssh_namelist_valid (msg->lists[i])) return 0;
  if (!mulib_ssh_write_u8 (&w, MULIB_SSH_KEXINIT)
      || !mulib_ssh_write_bytes (&w, msg->cookie, sizeof msg->cookie)) return 0;
  for (size_t i = 0; i < MULIB_SSH_LIST_COUNT; ++i)
    if (!mulib_ssh_write_string (&w, msg->lists[i].data, msg->lists[i].length))
      return 0;
  if (!mulib_ssh_write_u8 (&w, (unsigned char)msg->follows)
      || !mulib_ssh_write_u32 (&w, 0)) return 0;
  *used = cap - w.left;
  return 1;
}
#endif
