#include "mulib-sshconnection.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEXT(s) {(const unsigned char *)(s), sizeof (s) - 1}

static mulib_sshconnection_t client, server;
static mulib_sshchannel_slot_t client_slots[2], server_slots[2];
static const unsigned char host_seed[32] = {9}, alice_seed[32] = {7}, other_seed[32] = {8};
static unsigned char alice_key[32], other_key[32];
static const mulib_ssh_slice_t alice = TEXT("alice"), bob = TEXT("bob"), empty = {NULL, 0};
static const mulib_ssh_slice_t good_password = TEXT("a\0password"), bad_password = TEXT("wrong");

typedef struct {
  size_t queries, signatures, passwords;
  int allow_signed;
} calls_t;
static calls_t policy_calls;

static int
authorize (void *context, const mulib_ssh_auth_request_t *r)
{
  calls_t *calls = context;
  if (r->signature.length) ++calls->signatures;
  else ++calls->queries;
  return mulib_ssh_slice_is (r->user, "alice") && r->public_key.length == sizeof alice_key
    && memcmp (r->public_key.data, alice_key, sizeof alice_key) == 0
    && (!r->signature.length || calls->allow_signed);
}

static int
verify_password (void *context, const mulib_ssh_auth_request_t *r)
{
  ++((calls_t *)context)->passwords;
  return mulib_ssh_slice_is (r->user, "bob") && r->password.length == good_password.length
    && memcmp (r->password.data, good_password.data, r->password.length) == 0;
}

static void
start (size_t limit, size_t server_slot_count, int wrong_pin)
{
  unsigned char host_pin[32];
  mulib_ssh_slice_t cv = TEXT("SSH-2.0-mulib-client comment");
  mulib_ssh_slice_t sv = TEXT("SSH-2.0-mulib-server another comment");
  mulib_sshserver_policy_t policy = {limit, verify_password, authorize, &policy_calls};
  memset (&policy_calls, 0, sizeof policy_calls);
  policy_calls.allow_signed = 1;
  assert (mulib_ed25519_public (host_pin, host_seed));
  if (wrong_pin) host_pin[0] ^= 1;
  assert (mulib_sshconnection_start (&client, MULIB_SSH_CLIENT, cv, host_pin, NULL, client_slots, 2));
  assert (mulib_sshconnection_start (&server, MULIB_SSH_SERVER, sv, host_seed, &policy,
                                     server_slots, server_slot_count));
}

/* Pump one fragment, or drain already buffered input. */
static int
move (mulib_sshconnection_t *sender, mulib_sshconnection_t *receiver, size_t fragment,
      mulib_sshconnection_event_t *event)
{
  size_t used;
  mulib_ssh_slice_t wire = mulib_sshconnection_output (sender);
  if (wire.length > fragment) wire.length = fragment;
  int status = mulib_sshconnection_feed (receiver, wire.data, wire.length, &used, event);
  assert (used <= wire.length);
  if (status != -1) assert (mulib_sshconnection_consume_output (sender, used));
  return status == 0 && used ? 2 : status;
}

static void
drain (size_t fragment)
{
  mulib_sshconnection_event_t event;
  for (unsigned i = 0; i < 10000; ++i) {
    int a = move (&client, &server, fragment, &event), b = move (&server, &client, fragment, &event);
    assert (a >= 0 && b >= 0);
    if (!a && !b) return;
  }
  assert (0 && "no progress bound");
}

static void
with_ident_banner (void)
{
  static const unsigned char banner[] = "first warning\nsecond warning\r\n";
  unsigned char input[128];
  size_t used;
  mulib_sshconnection_event_t e;
  mulib_ssh_slice_t wire = mulib_sshconnection_output (&server);
  assert (sizeof banner - 1 + wire.length <= sizeof input);
  memcpy (input, banner, sizeof banner - 1);
  memcpy (input + sizeof banner - 1, wire.data, wire.length);
  assert (!mulib_sshconnection_consume_output (&server, wire.length + 1));
  assert (mulib_sshconnection_consume_output (&server, wire.length));
  /* While local identification is being written, hold coalesced input. */
  assert (!mulib_sshconnection_feed (&client, input, sizeof banner - 1 + wire.length, &used, &e));
  assert (used == sizeof banner - 1 + wire.length);
  wire = mulib_sshconnection_output (&client);
  assert (mulib_sshconnection_feed (&server, wire.data, wire.length, &used, &e) == 1);
  assert (mulib_sshconnection_consume_output (&client, used));
  assert (mulib_sshconnection_feed (&client, NULL, 0, &used, &e) == 1 && !used);
  assert (e.type == MULIB_SSH_CONNECTION_IDENT_BANNER && mulib_ssh_slice_is (e.text, "first warning"));
  assert (mulib_sshconnection_feed (&client, NULL, 0, &used, &e) == 1 && !used);
  assert (e.type == MULIB_SSH_CONNECTION_IDENT_BANNER && mulib_ssh_slice_is (e.text, "second warning"));
}

static void
send_auth (mulib_sshconnection_auth_kind_t kind, mulib_ssh_slice_t user,
           mulib_ssh_slice_t credential, mulib_sshconnection_event_t *reply)
{
  mulib_sshconnection_event_t event;
  assert (mulib_sshconnection_can_auth (&client));
  assert (mulib_sshconnection_auth (&client, kind, user, credential) == 1);
  assert (!mulib_sshconnection_can_auth (&client));
  assert (mulib_sshconnection_auth (&client, kind, user, credential) == 0);
  assert (move (&client, &server, SIZE_MAX, &event) == 1);
  if (event.type == MULIB_SSH_CONNECTION_AUTH_REPLY)
    assert (event.auth.type == MULIB_SSH_USERAUTH_SUCCESS);
  assert (move (&server, &client, SIZE_MAX, reply) == 1);
}

static void
test_auth (void)
{
  mulib_sshconnection_event_t e;
  mulib_ssh_slice_t seed = {alice_seed, sizeof alice_seed};
  mulib_ssh_slice_t key = {alice_key, sizeof alice_key}, bad_key = {other_key, sizeof other_key};
  mulib_ssh_slice_t wrong_seed = {other_seed, sizeof other_seed};
  mulib_ssh_slice_t message = TEXT("Authorized use only"), language = TEXT("en");
  mulib_sshchannel_command_t cmd = {0, MULIB_SSH_CHANNEL_OPEN, {11, 20, 10}, 0, 0, {NULL, 0}, {NULL, 0}};
  start (12, 2, 0);
  assert (mulib_sshconnection_auth (&client, MULIB_SSH_CONNECTION_AUTH_SIGNED, alice, seed) == 0);
  assert (mulib_sshconnection_channel (&client, &cmd) == 0);
  with_ident_banner ();
  drain (7);
  assert (mulib_sshconnection_can_auth (&client));
  assert (mulib_sshconnection_banner (&server, message, language) == 1);
  assert (mulib_sshconnection_auth (&client, MULIB_SSH_CONNECTION_AUTH_NONE, alice, empty) == 1);
  /* The banner remains legal while an auth reply is pending. */
  assert (move (&server, &client, SIZE_MAX, &e) == 2);
  assert (move (&client, &server, SIZE_MAX, &e) == 1);
  assert (!mulib_sshconnection_can_auth (&client));
  assert (mulib_sshconnection_feed (&client, NULL, 0, &(size_t){0}, &e) == 1);
  assert (e.type == MULIB_SSH_CONNECTION_AUTH_REPLY && e.auth.type == MULIB_SSH_USERAUTH_BANNER
          && mulib_ssh_slice_is (e.auth.message, "Authorized use only"));
  assert (!mulib_sshconnection_can_auth (&client));
  assert (move (&server, &client, SIZE_MAX, &e) == 1 && e.auth.type == MULIB_SSH_USERAUTH_FAILURE);
  assert (mulib_ssh_namelist_has (e.auth.methods, "password") && mulib_ssh_namelist_has (e.auth.methods, "publickey"));

  send_auth (MULIB_SSH_CONNECTION_AUTH_QUERY, alice, bad_key, &e);
  assert (e.auth.type == MULIB_SSH_USERAUTH_FAILURE);
  send_auth (MULIB_SSH_CONNECTION_AUTH_QUERY, alice, key, &e);
  assert (e.type == MULIB_SSH_CONNECTION_AUTH_PK_OK && e.text.length == sizeof alice_key
          && !memcmp (e.text.data, alice_key, sizeof alice_key));
  assert (!mulib_sshconnection_authenticated (&client) && !mulib_sshconnection_authenticated (&server));
  assert (!mulib_sshconnection_user (&server).length);
  send_auth (MULIB_SSH_CONNECTION_AUTH_SIGNED, bob, seed, &e); /* Same key, different account. */
  assert (e.auth.type == MULIB_SSH_USERAUTH_FAILURE);
  send_auth (MULIB_SSH_CONNECTION_AUTH_SIGNED, alice, wrong_seed, &e); /* Same account, different key. */
  assert (e.auth.type == MULIB_SSH_USERAUTH_FAILURE);
  policy_calls.allow_signed = 0; /* Revoke even for the exact queried account/key. */
  send_auth (MULIB_SSH_CONNECTION_AUTH_SIGNED, alice, seed, &e);
  assert (e.auth.type == MULIB_SSH_USERAUTH_FAILURE);
  send_auth (MULIB_SSH_CONNECTION_AUTH_PASSWORD, bob, bad_password, &e);
  assert (e.auth.type == MULIB_SSH_USERAUTH_FAILURE);
  policy_calls.allow_signed = 1;
  send_auth (MULIB_SSH_CONNECTION_AUTH_SIGNED, alice, seed, &e);
  assert (e.auth.type == MULIB_SSH_USERAUTH_SUCCESS && mulib_sshconnection_authenticated (&client)
          && mulib_sshconnection_authenticated (&server));
  assert (mulib_ssh_slice_is (mulib_sshconnection_user (&server), "alice"));
  assert (mulib_ssh_slice_is (mulib_sshconnection_user (&client), "alice"));
  assert (policy_calls.queries == 2 && policy_calls.signatures == 4 && policy_calls.passwords == 1);
  mulib_sshconnection_clear (&client);
  mulib_sshconnection_clear (&server);

  start (2, 2, 0);
  drain (SIZE_MAX);
  send_auth (MULIB_SSH_CONNECTION_AUTH_QUERY, alice, key, &e);
  assert (e.type == MULIB_SSH_CONNECTION_AUTH_PK_OK);
  send_auth (MULIB_SSH_CONNECTION_AUTH_QUERY, alice, key, &e);
  assert (e.auth.type == MULIB_SSH_USERAUTH_FAILURE && !e.auth.methods.length);
  assert (!mulib_sshconnection_can_auth (&client) && policy_calls.queries == 1);
  mulib_sshconnection_clear (&client);
  mulib_sshconnection_clear (&server);
}

static void
raw_payload (mulib_sshconnection_t *s, const void *payload, size_t size)
{
  assert (!mulib_sshconnection_output (s).length);
  assert (mulib_sshtransport_seal (&s->transport, payload, size, s->tx, sizeof s->tx, &s->tx_end) == 1);
}

static void
test_auth_rejections (void)
{
  unsigned char payload[2048], foreign_session[32];
  size_t n;
  mulib_sshconnection_event_t event;
  mulib_ssh_slice_t key = {alice_key, sizeof alice_key};
  start (4, 2, 0);
  drain (SIZE_MAX);
  assert (mulib_sshtransport_session_id (&client.transport, foreign_session));
  foreign_session[0] ^= 1;
  assert (mulib_sshauth_public_write (alice, alice_seed, foreign_session, payload, sizeof payload, &n));
  raw_payload (&client, payload, n);
  assert (move (&client, &server, SIZE_MAX, &event) == 1 && !policy_calls.signatures);
  mulib_sshconnection_clear (&client);
  mulib_sshconnection_clear (&server);

  start (4, 2, 0);
  drain (SIZE_MAX);
  assert (mulib_sshconnection_auth (&client, MULIB_SSH_CONNECTION_AUTH_QUERY, alice, key) == 1);
  assert (mulib_sshconnection_consume_output (&client, mulib_sshconnection_output (&client).length));
  assert (mulib_sshauth_pk_ok_write (other_key, payload, sizeof payload, &n));
  raw_payload (&server, payload, n);
  event.type = MULIB_SSH_CONNECTION_IDENT_BANNER;
  assert (move (&server, &client, SIZE_MAX, &event) == -1);
  assert (event.type == MULIB_SSH_CONNECTION_IDENT_BANNER && !mulib_sshconnection_authenticated (&client));
  mulib_sshconnection_clear (&client);
  mulib_sshconnection_clear (&server);

  start (4, 2, 0);
  drain (SIZE_MAX);
  assert (mulib_sshauth_service_write (0, payload, sizeof payload, &n));
  raw_payload (&client, payload, n); /* Duplicate SERVICE_REQUEST, no verifier. */
  assert (move (&client, &server, SIZE_MAX, &event) == -1 && !policy_calls.passwords);
  mulib_sshconnection_clear (&client);
  mulib_sshconnection_clear (&server);

  start (4, 2, 0);
  drain (SIZE_MAX);
  mulib_ssh_channel_endpoint_t ep = {11, 20, 10};
  assert (mulib_sshchannel_open_write (ep, payload, sizeof payload, &n));
  raw_payload (&client, payload, n); /* Channel traffic is outside the auth phase. */
  assert (move (&client, &server, SIZE_MAX, &event) == -1);
  assert (server_slots[0].state == MULIB_SSH_CHANNEL_FREE);
  mulib_sshconnection_clear (&client);
  mulib_sshconnection_clear (&server);

  start (4, 2, 0);
  drain (SIZE_MAX);
  assert (mulib_sshconnection_auth (&client, MULIB_SSH_CONNECTION_AUTH_PASSWORD, bob, good_password) == 1);
  client.tx[client.tx_end - 1] ^= 1; /* Bad AEAD tag must not reach the verifier. */
  event.type = MULIB_SSH_CONNECTION_IDENT_BANNER;
  assert (move (&client, &server, SIZE_MAX, &event) == -1 && !policy_calls.passwords);
  assert (event.type == MULIB_SSH_CONNECTION_IDENT_BANNER && !mulib_sshconnection_output (&server).length);
  mulib_sshconnection_clear (&client);
  mulib_sshconnection_clear (&server);

  start (4, 2, 1);
  for (unsigned i = 0; i < 32; ++i) {
    assert (move (&client, &server, SIZE_MAX, &event) >= 0);
    if (move (&server, &client, SIZE_MAX, &event) == -1) {
      assert (!mulib_sshconnection_output (&client).length && !mulib_sshconnection_can_auth (&client));
      mulib_sshconnection_clear (&client);
      mulib_sshconnection_clear (&server);
      return;
    }
  }
  assert (0 && "wrong pin must close");
}

static void
test_dispatch_retries (void)
{
  unsigned char payload[128];
  size_t n;
  mulib_sshchannel_slot_t local[2] = {0}, peer[2] = {0};
  mulib_sshchannel_dispatch_event_t event;
  mulib_sshchannel_command_t open = {0, MULIB_SSH_CHANNEL_OPEN, {11, 5, 5}, 0, 0, {NULL, 0}, {NULL, 0}};
  mulib_sshchannel_command_t reply = {0, MULIB_SSH_CHANNEL_OPEN_CONFIRMATION, {22, 6, 6},
                                     0, 0, {NULL, 0}, {NULL, 0}};
  assert (!mulib_sshchannel_dispatch_send (local, 2, &open, payload, 4, &n));
  assert (!n && local[0].state == MULIB_SSH_CHANNEL_FREE);
  assert (mulib_sshchannel_dispatch_send (local, 2, &open, payload, sizeof payload, &n));
  event.slot = SIZE_MAX;
  assert (!mulib_sshchannel_dispatch_receive (peer, 2, payload, n - 1, &event));
  assert (event.slot == SIZE_MAX && peer[0].state == MULIB_SSH_CHANNEL_FREE);
  assert (mulib_sshchannel_dispatch_receive (peer, 2, payload, n, &event));
  assert (event.slot == 0 && peer[0].state == MULIB_SSH_CHANNEL_OPEN_RECEIVED);
  assert (!mulib_sshchannel_dispatch_receive (peer, 2, payload, n, &event));
  assert (peer[1].state == MULIB_SSH_CHANNEL_FREE);
  assert (!mulib_sshchannel_dispatch_send (peer, 2, &reply, payload, 16, &n));
  assert (!n && peer[0].state == MULIB_SSH_CHANNEL_OPEN_RECEIVED);
  assert (mulib_sshchannel_dispatch_send (peer, 2, &reply, payload, sizeof payload, &n));
  payload[n] = 0;
  assert (!mulib_sshchannel_dispatch_receive (local, 2, payload, n + 1, &event));
  assert (local[0].state == MULIB_SSH_CHANNEL_OPEN_SENT);
  assert (mulib_sshchannel_dispatch_receive (local, 2, payload, n, &event));
  assert (local[0].state == MULIB_SSH_CHANNEL_ACTIVE);
  payload[0] = MULIB_SSH_CHANNEL_SUCCESS;
  assert (!mulib_sshchannel_dispatch_receive (local, 2, payload, 5, &event));
  assert (!local[0].replies_expected && event.message.type == MULIB_SSH_CHANNEL_OPEN_CONFIRMATION);
  reply.type = MULIB_SSH_CHANNEL_CLOSE;
  assert (mulib_sshchannel_dispatch_send (peer, 2, &reply, payload, sizeof payload, &n));
  assert (mulib_sshchannel_dispatch_receive (local, 2, payload, n, &event) && !event.released);
  open.slot = 1;
  assert (!mulib_sshchannel_dispatch_send (local, 2, &open, payload, sizeof payload, &n)); /* ID still reserved. */
  open.slot = 0;
  open.type = MULIB_SSH_CHANNEL_CLOSE;
  assert (!mulib_sshchannel_dispatch_send (local, 2, &open, payload, 4, &n));
  assert (local[0].state == MULIB_SSH_CHANNEL_ACTIVE && local[0].channel.received_close);
  assert (mulib_sshchannel_dispatch_send (local, 2, &open, payload, sizeof payload, &n));
  assert (mulib_sshchannel_dispatch_receive (peer, 2, payload, n, &event) && event.released);
  assert (local[0].state == MULIB_SSH_CHANNEL_FREE && peer[0].state == MULIB_SSH_CHANNEL_FREE);
}

static mulib_sshchannel_dispatch_event_t
channel (mulib_sshconnection_t *sender, mulib_sshconnection_t *receiver,
         const mulib_sshchannel_command_t *command)
{
  mulib_sshconnection_event_t event;
  assert (mulib_sshconnection_channel (sender, command) == 1);
  assert (move (sender, receiver, SIZE_MAX, &event) == 1 && event.type == MULIB_SSH_CONNECTION_CHANNEL);
  return event.channel;
}

static void
test_channels (void)
{
  mulib_sshconnection_event_t auth;
  mulib_sshchannel_dispatch_event_t e;
  mulib_sshchannel_command_t a = {0, MULIB_SSH_CHANNEL_OPEN, {11, 5, 5}, 0, 0, {NULL, 0}, {NULL, 0}};
  mulib_sshchannel_command_t b = {0, MULIB_SSH_CHANNEL_OPEN_FAILURE, {22, 6, 6}, 1, 0, {NULL, 0}, {NULL, 0}};
  mulib_ssh_slice_t denied = TEXT("denied"), data = TEXT("abc"), request = TEXT("opaque@example.com");
  start (3, 1, 0);
  drain (SIZE_MAX);
  send_auth (MULIB_SSH_CONNECTION_AUTH_PASSWORD, bob, good_password, &auth);
  assert (auth.auth.type == MULIB_SSH_USERAUTH_SUCCESS && policy_calls.passwords == 1);
  assert (mulib_ssh_slice_is (mulib_sshconnection_user (&server), "bob"));

  e = channel (&client, &server, &a);
  assert (e.slot == 0 && e.endpoint.id == 11 && server_slots[0].state == MULIB_SSH_CHANNEL_OPEN_RECEIVED);
  a.type = MULIB_SSH_CHANNEL_DATA;
  a.data = data;
  assert (!mulib_sshconnection_channel (&client, &a)); /* Wait for open confirmation. */
  b.data = denied;
  e = channel (&server, &client, &b);
  assert (e.released && e.message.value == 1 && mulib_ssh_slice_is (e.message.data, "denied"));

  a.type = MULIB_SSH_CHANNEL_OPEN;
  channel (&client, &server, &a);
  b.type = MULIB_SSH_CHANNEL_OPEN_CONFIRMATION;
  e = channel (&server, &client, &b);
  assert (e.slot == 0 && e.endpoint.id == 22 && client_slots[0].state == MULIB_SSH_CHANNEL_ACTIVE);
  a.slot = 1;
  assert (!mulib_sshconnection_channel (&client, &a)); /* Duplicate local wire ID. */
  a.endpoint.id = 33;
  e = channel (&client, &server, &a);
  assert (e.slot == SIZE_MAX && e.message.value == MULIB_SSH_OPEN_RESOURCE_SHORTAGE);
  assert (move (&server, &client, SIZE_MAX, &auth) == 1 && auth.channel.released && auth.channel.slot == 1);
  a.slot = 0;

  a.type = MULIB_SSH_CHANNEL_EXTENDED_DATA;
  a.value = 1;
  e = channel (&client, &server, &a);
  assert (e.message.value == 1 && mulib_ssh_slice_is (e.message.data, "abc"));
  a.type = MULIB_SSH_CHANNEL_DATA;
  channel (&client, &server, &a);
  assert (!mulib_sshconnection_channel (&client, &a));
  b.type = MULIB_SSH_CHANNEL_WINDOW_ADJUST;
  b.value = 7;
  assert (!mulib_sshconnection_channel (&server, &b));
  b.value = 3;
  channel (&server, &client, &b);
  channel (&client, &server, &a);
  assert (!mulib_sshconnection_channel (&client, &a));

  a.type = MULIB_SSH_CHANNEL_REQUEST;
  a.name = request;
  a.want_reply = 1;
  e = channel (&client, &server, &a);
  assert (e.message.want_reply && mulib_ssh_slice_is (e.message.name, "opaque@example.com"));
  channel (&client, &server, &a);
  b.type = MULIB_SSH_CHANNEL_SUCCESS;
  channel (&server, &client, &b);
  b.type = MULIB_SSH_CHANNEL_FAILURE;
  channel (&server, &client, &b);
  assert (!mulib_sshconnection_channel (&server, &b));
  assert (!client_slots[0].replies_expected && !server_slots[0].replies_to_send);

  /* Queue both directions before reading either CLOSE. */
  a.type = b.type = MULIB_SSH_CHANNEL_CLOSE;
  assert (mulib_sshconnection_channel (&client, &a) == 1);
  assert (mulib_sshconnection_channel (&server, &b) == 1);
  assert (move (&client, &server, 3, &auth) == 2);
  assert (move (&server, &client, SIZE_MAX, &auth) == 2);
  assert (client_slots[0].state == MULIB_SSH_CHANNEL_ACTIVE && server_slots[0].state == MULIB_SSH_CHANNEL_ACTIVE);
  assert (move (&client, &server, SIZE_MAX, &auth) == 1 && auth.channel.released);
  assert (mulib_sshconnection_feed (&client, NULL, 0, &(size_t){0}, &auth) == 1 && auth.channel.released);
  assert (client_slots[0].state == MULIB_SSH_CHANNEL_FREE && server_slots[0].state == MULIB_SSH_CHANNEL_FREE);
  a.type = MULIB_SSH_CHANNEL_OPEN;
  a.endpoint.id = 11;
  /* Simulate a peer starting transport rekey without the connection driver's
   * assistance. Preserve the authenticated identity and channel dispatch. */
  assert (mulib_sshtransport_rekey (&server.transport, server.tx, sizeof server.tx, &server.tx_end) == 1);
  assert (move (&server, &client, SIZE_MAX, &auth) == 1 && auth.type == MULIB_SSH_CONNECTION_NONE);
  assert (!mulib_sshconnection_channel (&client, &a));
  drain (SIZE_MAX);
  assert (mulib_ssh_slice_is (mulib_sshconnection_user (&server), "bob"));
  channel (&client, &server, &a);
  mulib_sshconnection_clear (&client);
  mulib_sshconnection_clear (&server);
}

int
main (void)
{
  assert (mulib_ed25519_public (alice_key, alice_seed));
  assert (mulib_ed25519_public (other_key, other_seed));
  test_auth ();
  test_auth_rejections ();
  test_dispatch_retries ();
  test_channels ();
  puts ("SSH connection tests passed");
  return 0;
}
