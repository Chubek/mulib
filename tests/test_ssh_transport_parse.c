/* Standalone seeded mutation test; define MULIB_SSH_LIBFUZZER for libFuzzer. */
#include "mulib-sshtransport.h"
#include <assert.h>
#include <string.h>

static const unsigned char sentinel[] = {0};

static void
check (const unsigned char *p, size_t n)
{
  mulib_ssh_slice_t v = {sentinel, 123}, w = v, x = v;
  mulib_ssh_reader_t r = {p, n};
  if (!mulib_ssh_read_string (&r, &v)) {
    assert (r.next == p && r.left == n && v.data == sentinel && v.length == 123);
  } else {
    assert (n >= 4 && v.length <= n - 4);
    assert (v.data == p + 4 && r.next == p + 4 + v.length && r.left == n - 4 - v.length);
  }
  r = (mulib_ssh_reader_t){p, n};
  v = w;
  if (!mulib_ssh_read_mpint (&r, &v)) {
    assert (r.next == p && r.left == n && v.data == sentinel && v.length == 123);
  } else {
    assert (n >= 4 && v.length <= n - 4 && (!v.length || v.data[0]));
    assert (v.data == p + 4 || v.data == p + 5);
  }
  for (int encrypted = 0; encrypted <= 1; ++encrypted) {
    size_t body = 555, frame = 777;
    if (mulib_sshpacket_peek (p, n, encrypted, &body, &frame) != 1) {
      assert (body == 555 && frame == 777);
    } else {
      assert (frame <= n && frame == body + (encrypted ? 20 : 4));
      assert (body >= 6 && body <= MULIB_SSH_BODY_MAX);
      v = w;
      if (mulib_sshpacket_payload (p + 4, body, &v)) {
        assert (v.data == p + 5 && v.length && v.length <= MULIB_SSH_PAYLOAD_MAX);
      } else {
        assert (v.data == sentinel && v.length == 123);
      }
    }
  }
  mulib_ssh_kexinit_t init;
  unsigned char old[sizeof init];
  memset (&init, 0xa5, sizeof init);
  memcpy (old, &init, sizeof old);
  if (!mulib_ssh_kexinit_parse (p, n, &init)) {
    assert (!memcmp (&init, old, sizeof old));
  } else {
    for (size_t i = 0; i < MULIB_SSH_LIST_COUNT; ++i)
      assert (mulib_ssh_namelist_valid (init.lists[i]));
  }
  v = w;
  if (mulib_sshkey_public_parse (p, n, &v)) {
    assert (n == MULIB_SSH_ED25519_KEY_BLOB && v.data == p + 19 && v.length == 32);
  } else {
    assert (v.data == sentinel && v.length == 123);
  }
  v = w;
  if (mulib_sshkey_signature_parse (p, n, &v)) {
    assert (n == MULIB_SSH_ED25519_SIG_BLOB && v.data == p + 19 && v.length == 64);
  } else {
    assert (v.data == sentinel && v.length == 123);
  }
  v = w;
  if (mulib_sshkex_init_parse (p, n, &v)) {
    assert (n == MULIB_SSH_ECDH_INIT_SIZE && v.data == p + 5 && v.length == 32);
  } else {
    assert (v.data == sentinel && v.length == 123);
  }
  mulib_ssh_slice_t before = v;
  if (mulib_sshkex_reply_parse (p, n, &v, &w, &x)) {
    assert (n == MULIB_SSH_ECDH_REPLY_SIZE && v.length == 32 && w.length == 32 && x.length == 64);
  } else {
    assert (v.data == before.data && v.length == before.length);
    assert (w.data == sentinel && w.length == 123 && x.data == sentinel && x.length == 123);
  }
}

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  check (data, size);
  /* Also examine payloads after a packet header and the next coalesced record. */
  if (size >= 5) check (data + 5, size - 5);
  for (int encrypted = 0; encrypted <= 1; ++encrypted) {
    size_t body, frame;
    if (mulib_sshpacket_peek (data, size, encrypted, &body, &frame) == 1) {
      mulib_ssh_slice_t payload;
      if (mulib_sshpacket_payload (data + 4, body, &payload))
        check (payload.data, payload.length);
      check (data + frame, size - frame);
    }
  }
  return 0;
}

#ifndef MULIB_SSH_LIBFUZZER
static int
fixed_random (void *context, void *out, size_t n)
{
  (void)context;
  memset (out, 0x51, n);
  return 1;
}

static void
mutate (unsigned char *p, size_t n)
{
  static const unsigned char corruption[] = {0, 0xff, 1, 0x80, ',', 0x20, 0x7f};
  p[n] = 0xff;
  for (size_t i = 0; i <= n + 1; ++i) LLVMFuzzerTestOneInput (p, i);
  for (size_t i = 0; i < n; ++i) {
    unsigned char before = p[i];
    for (size_t j = 0; j < sizeof corruption; ++j) {
      p[i] = corruption[j];
      LLVMFuzzerTestOneInput (p, n);
    }
    p[i] = before;
  }
}

int
main (void)
{
  unsigned char raw[64];
  unsigned char p[MULIB_SSH_KEX_OUTPUT_MIN], wrapped[2 * MULIB_SSH_KEX_OUTPUT_MIN + 1];
  const unsigned char max_string[] = {0xff, 0xff, 0xff, 0xff};
  size_t n, first, second;
  memset (raw, 0x81, sizeof raw);
  assert (mulib_sshkex_propose (raw, p, sizeof p, &n));
  mutate (p, n);
  for (int encrypted = 0; encrypted <= 1; ++encrypted) {
    assert (mulib_sshpacket_write (p, n, encrypted, wrapped, sizeof wrapped, &first,
                                    fixed_random, NULL));
    assert (mulib_sshpacket_write (p, n, 0, wrapped + first + (encrypted ? 16 : 0),
                                    sizeof wrapped - first - (encrypted ? 16 : 0), &second,
                                    fixed_random, NULL));
    if (encrypted) memset (wrapped + first, 0x51, 16);
    mutate (wrapped, first + (encrypted ? 16 : 0) + second);
  }
  assert (mulib_sshkey_public_write (raw, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshkey_signature_write (raw, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshkex_init_write (raw, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshkex_reply_write (raw, raw, raw, p, sizeof p, &n));
  mutate (p, n);
  mulib_ssh_writer_t cursor = {p, sizeof p};
  assert (mulib_ssh_write_mpint (&cursor, raw, sizeof raw));
  mutate (p, sizeof p - cursor.left);
  LLVMFuzzerTestOneInput (max_string, sizeof max_string);
  return 0;
}
#endif
