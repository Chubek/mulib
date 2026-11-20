/* Standalone seeded mutation test; define MULIB_SSH_LIBFUZZER for libFuzzer. */
#include "mulib-sshauth.h"
#include "mulib-sshchannel.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

static const unsigned char session_id[32] = {0x12, 0x34};

static void
borrowed (mulib_ssh_slice_t s, const unsigned char *data, size_t n)
{
  if (!s.data) {
    assert (!s.length);
  } else {
    uintptr_t p = (uintptr_t)s.data, first = (uintptr_t)data;
    assert (p >= first && p - first <= n && s.length <= n - (p - first));
  }
}

static void
check_auth (const unsigned char *p, size_t n)
{
  mulib_ssh_auth_request_t request;
  mulib_ssh_auth_reply_t reply;
  unsigned char old_request[sizeof request], old_reply[sizeof reply];
  mulib_ssh_slice_t key = {session_id, sizeof session_id};
  memset (&request, 0xab, sizeof request);
  memset (&reply, 0xab, sizeof reply);
  memcpy (old_request, &request, sizeof request);
  memcpy (old_reply, &reply, sizeof reply);
  if (mulib_sshauth_parse (p, n, &request) == 1) {
    assert (request.user.length && request.user.length <= MULIB_SSH_USER_MAX);
    assert (request.method >= MULIB_SSH_AUTH_NONE && request.method <= MULIB_SSH_AUTH_ED25519);
    assert (request.password.length <= MULIB_SSH_PASSWORD_MAX);
    assert (request.public_key.length == (request.method == MULIB_SSH_AUTH_ED25519 ? 32u : 0u));
    assert (!request.signature.length || request.signature.length == 64);
    borrowed (request.user, p, n);
    borrowed (request.password, p, n);
    borrowed (request.public_key, p, n);
    borrowed (request.signature, p, n);
    if (request.signature.length) {
      mulib_ssh_auth_request_t verified;
      memset (&verified, 0xab, sizeof verified);
      if (mulib_sshauth_public_verify (p, n, session_id, &verified))
        assert (request.user.data == verified.user.data
                && request.public_key.data == verified.public_key.data
                && request.signature.data == verified.signature.data);
      else assert (!memcmp (&verified, old_request, sizeof old_request));
    }
  } else assert (!memcmp (&request, old_request, sizeof old_request));
  if (mulib_sshauth_reply_parse (p, n, &reply)) {
    assert (reply.type == MULIB_SSH_USERAUTH_SUCCESS || reply.type == MULIB_SSH_USERAUTH_FAILURE
            || reply.type == MULIB_SSH_USERAUTH_BANNER);
    borrowed (reply.methods, p, n);
    borrowed (reply.message, p, n);
    borrowed (reply.language, p, n);
  } else assert (!memcmp (&reply, old_reply, sizeof old_reply));
  if (mulib_sshauth_pk_ok_parse (p, n, &key)) {
    assert (key.length == 32);
    borrowed (key, p, n);
  } else assert (key.data == session_id && key.length == sizeof session_id);
  for (int accept = 0; accept <= 1; ++accept)
    if (mulib_sshauth_service_parse (accept, p, n)) assert (n == 17);
}

static void
check_channel (const unsigned char *p, size_t n)
{
  mulib_sshchannel_slot_t slots[3] = {0};
  unsigned char before[sizeof slots];
  mulib_ssh_channel_endpoint_t ep = {1, 32, 16}, answer = {9, 16, 16};
  mulib_sshchannel_dispatch_event_t event;
  unsigned char old_event[sizeof event];
  slots[0].state = MULIB_SSH_CHANNEL_ACTIVE;
  assert (mulib_sshchannel_init (&slots[0].channel, ep, answer));
  slots[0].replies_expected = 1;
  slots[1].state = MULIB_SSH_CHANNEL_OPEN_SENT;
  slots[1].channel.local = (mulib_ssh_channel_endpoint_t){2, 33, 16};
  memcpy (before, slots, sizeof before);
  memset (&event, 0xab, sizeof event);
  memcpy (old_event, &event, sizeof old_event);
  if (!mulib_sshchannel_dispatch_receive (slots, 3, p, n, &event)) {
    assert (!memcmp (before, slots, sizeof before) && !memcmp (old_event, &event, sizeof old_event));
    return;
  }
  assert (n && event.message.type == p[0]);
  borrowed (event.message.name, p, n);
  borrowed (event.message.data, p, n);
  if (event.slot == SIZE_MAX) {
    assert (event.message.type == MULIB_SSH_CHANNEL_OPEN && event.message.value == MULIB_SSH_OPEN_UNKNOWN_CHANNEL_TYPE);
    assert (!memcmp (before, slots, sizeof before));
    return;
  }
  assert (event.slot < 3);
  for (size_t i = 0; i < 3; ++i)
    if (i != event.slot) assert (!memcmp (&slots[i], before + i * sizeof slots[i], sizeof slots[i]));
  const mulib_sshchannel_slot_t *s = &slots[event.slot];
  if (event.message.type == MULIB_SSH_CHANNEL_DATA || event.message.type == MULIB_SSH_CHANNEL_EXTENDED_DATA) {
    assert (event.slot == 0 && event.message.data.length <= ep.max_packet);
    assert (s->channel.local.window + s->uncredited == ep.window);
  }
  if (event.released) assert (s->state == MULIB_SSH_CHANNEL_FREE);
}

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  check_auth (data, size);
  check_channel (data, size);
  size_t body, frame;
  for (unsigned i = 0; i < 2 && mulib_sshpacket_peek (data, size, 0, &body, &frame) == 1; ++i) {
    mulib_ssh_slice_t p;
    if (mulib_sshpacket_payload (data + 4, body, &p)) {
      check_auth (p.data, p.length);
      check_channel (p.data, p.length);
    }
    data += frame;
    size -= frame;
  }
  return 0;
}

#ifndef MULIB_SSH_LIBFUZZER
static void
mutate (unsigned char *p, size_t n)
{
  static const unsigned char changes[] = {0, 1, 0x20, 0x7f, 0x80, 0xff};
  p[n] = 0x81;
  for (size_t i = 0; i <= n + 1; ++i) LLVMFuzzerTestOneInput (p, i);
  for (size_t i = 0; i < n; ++i) {
    unsigned char saved = p[i];
    for (size_t j = 0; j < sizeof changes; ++j) {
      p[i] = changes[j];
      LLVMFuzzerTestOneInput (p, n);
    }
    p[i] = saved;
  }
}

static int
fixed_padding (void *context, void *out, size_t n)
{
  (void)context;
  memset (out, 0xcb, n);
  return 1;
}

int
main (void)
{
  unsigned char p[MULIB_SSH_PASSWORD_MAX + MULIB_SSH_USER_MAX + 80];
  unsigned char raw_password[MULIB_SSH_PASSWORD_MAX], raw_user[MULIB_SSH_USER_MAX], key[32];
  unsigned char coalesced[512];
  const unsigned char seed[32] = {7};
  const mulib_ssh_slice_t user = {(const unsigned char *)"alice", 5};
  const mulib_ssh_slice_t request = {(const unsigned char *)"exec", 4};
  const mulib_ssh_slice_t methods = {(const unsigned char *)"publickey,password", 18};
  const mulib_ssh_slice_t bytes = {(const unsigned char *)"foo\0bar", 7}, empty = {NULL, 0};
  size_t n, first, second;

  assert (mulib_sshauth_service_write (0, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshauth_service_write (1, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshauth_none_write (user, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshauth_password_write (user, bytes, p, sizeof p, &n));
  mutate (p, n);
  memset (raw_user, 'u', sizeof raw_user);
  memset (raw_password, 'p', sizeof raw_password);
  assert (mulib_sshauth_password_write ((mulib_ssh_slice_t){raw_user, sizeof raw_user},
                                        (mulib_ssh_slice_t){raw_password, sizeof raw_password}, p, sizeof p, &n));
  LLVMFuzzerTestOneInput (p, n);
  LLVMFuzzerTestOneInput (p, n - 1);
  assert (mulib_ed25519_public (key, seed));
  assert (mulib_sshauth_public_query (user, key, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshauth_public_write (user, seed, session_id, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshpacket_write (p, n, 0, coalesced, sizeof coalesced, &first, fixed_padding, NULL));
  assert (mulib_sshauth_pk_ok_write (key, p, sizeof p, &n));
  assert (mulib_sshpacket_write (p, n, 0, coalesced + first, sizeof coalesced - first,
                                 &second, fixed_padding, NULL));
  mutate (coalesced, first + second);
  assert (mulib_sshauth_pk_ok_write (key, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshauth_reply_write (0, methods, 0, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshauth_reply_write (1, empty, 0, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshauth_banner_write (bytes, empty, p, sizeof p, &n));
  mutate (p, n);

  mulib_ssh_channel_endpoint_t local = {4, 32, 16}, peer = {1, 32, 16};
  mulib_sshchannel_t channel;
  assert (mulib_sshchannel_open_write (local, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshchannel_confirm_write (2, local, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshchannel_failure_write (2, 1, bytes, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshchannel_init (&channel, local, peer));
  assert (mulib_sshchannel_data (&channel, 0, bytes, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshchannel_data (&channel, 1, bytes, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshchannel_window (&channel, 32, p, sizeof p, &n));
  mutate (p, n);
  assert (mulib_sshchannel_request (&channel, request, 1, bytes, p, sizeof p, &n));
  mutate (p, n);
  const unsigned char types[] = {MULIB_SSH_CHANNEL_SUCCESS, MULIB_SSH_CHANNEL_FAILURE,
                                 MULIB_SSH_CHANNEL_EOF, MULIB_SSH_CHANNEL_CLOSE};
  for (size_t i = 0; i < sizeof types; ++i) {
    assert (mulib_sshchannel_control (&channel, types[i], p, sizeof p, &n));
    mutate (p, n);
  }
  return 0;
}
#endif
