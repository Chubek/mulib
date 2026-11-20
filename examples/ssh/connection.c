/* In-memory driver example. Both seeds are public test data, not real keys. */
#include "implementation.h"
#include <stdio.h>
#include <string.h>

#define TEXT(s) { (const unsigned char *)(s), sizeof (s) - 1 }

static mulib_sshconnection_t client, server;
static mulib_sshchannel_slot_t client_slot, server_slot;

static int
authorize (void *context, const mulib_ssh_auth_request_t *request)
{
  return mulib_ssh_slice_is (request->user, "example-user")
    && request->public_key.length == 32 && !memcmp (request->public_key.data, context, 32);
}

/* Simulate socket chunks. A real event loop drives reads and writes separately. */
static int
step (mulib_sshconnection_t *from, mulib_sshconnection_t *to, mulib_sshconnection_event_t *event)
{
  mulib_ssh_slice_t output = mulib_sshconnection_output (from);
  size_t used;
  if (output.length > 31) output.length = 31;
  int progress = mulib_sshconnection_feed (to, output.data, output.length, &used, event);
  if (progress < 0 || !mulib_sshconnection_consume_output (from, used)) return -1;
  if (!progress) event->type = MULIB_SSH_CONNECTION_NONE;
  return progress || used != 0;
}

static int
drive (int *delivered)
{
  int in, out;
  do {
    mulib_sshconnection_event_t incoming, outgoing;
    in = step (&client, &server, &incoming);
    if (in < 0) return 0;
    if (incoming.type == MULIB_SSH_CONNECTION_CHANNEL) {
      if (incoming.channel.message.type == MULIB_SSH_CHANNEL_OPEN) {
        mulib_sshchannel_command_t accept = {
          .slot = incoming.channel.slot, .type = MULIB_SSH_CHANNEL_OPEN_CONFIRMATION,
          .endpoint = { 2, 4096, 1024 }
        };
        if (mulib_sshconnection_channel (&server, &accept) != 1) return 0;
      } else if (incoming.channel.message.type == MULIB_SSH_CHANNEL_DATA) {
        if (!mulib_ssh_slice_is (incoming.channel.message.data, "hello over SSH")) return 0;
        *delivered = 1;
      }
    }
    out = step (&server, &client, &outgoing);
    if (out < 0) return 0;
  } while (in || out);
  return 1;
}

int
main (void)
{
  const unsigned char host_seed[32] = { 1 }, user_seed[32] = { 2 };
  const mulib_ssh_slice_t vc = TEXT ("SSH-2.0-example-client");
  const mulib_ssh_slice_t vs = TEXT ("SSH-2.0-example-server");
  const mulib_ssh_slice_t user = TEXT ("example-user"), seed = { user_seed, sizeof user_seed };
  unsigned char host_pin[32], user_pin[32];
  const mulib_sshserver_policy_t policy = { 3, NULL, authorize, user_pin };
  const mulib_sshchannel_command_t open = {
    .type = MULIB_SSH_CHANNEL_OPEN, .endpoint = { 1, 4096, 1024 }
  };
  const mulib_sshchannel_command_t send = { .type = MULIB_SSH_CHANNEL_DATA, .data = TEXT ("hello over SSH") };
  const mulib_sshchannel_command_t close = { .type = MULIB_SSH_CHANNEL_CLOSE };
  int delivered = 0;
  int ok = mulib_ed25519_public (host_pin, host_seed)
    && mulib_ed25519_public (user_pin, user_seed)
    && mulib_sshconnection_start (&client, MULIB_SSH_CLIENT, vc, host_pin, NULL, &client_slot, 1)
    && mulib_sshconnection_start (&server, MULIB_SSH_SERVER, vs, host_seed, &policy, &server_slot, 1)
    && drive (&delivered) && mulib_sshconnection_can_auth (&client)
    && mulib_sshconnection_auth (&client, MULIB_SSH_CONNECTION_AUTH_SIGNED, user, seed) == 1
    && drive (&delivered) && mulib_sshconnection_authenticated (&client)
    && mulib_sshconnection_channel (&client, &open) == 1 && drive (&delivered)
    && mulib_sshconnection_channel (&client, &send) == 1 && drive (&delivered) && delivered
    && mulib_sshconnection_channel (&client, &close) == 1 && drive (&delivered)
    && mulib_sshconnection_channel (&server, &close) == 1 && drive (&delivered);
  mulib_sshconnection_clear (&server);
  mulib_sshconnection_clear (&client);
  if (!ok) {
    fputs ("SSH connection example failed\n", stderr);
    return 1;
  }
  puts ("Authenticated, transferred channel data, and closed using short writes.");
  return 0;
}
