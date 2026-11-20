/* In-memory SSH exchange. The fixed seeds are demonstration data only. */
#include "implementation.h"
#include <stdio.h>
#include <string.h>

#define SLICE(literal) { (const unsigned char *)(literal), sizeof (literal) - 1 }

typedef struct {
  unsigned char bytes[MULIB_SSH_FRAME_MAX];
  size_t length;
} frame_t;

static int
drive (mulib_sshtransport_t *receiver, frame_t *incoming, frame_t *outgoing)
{
  size_t used;
  outgoing->length = 0;
  while (incoming->length) {
    size_t written;
    if (mulib_sshtransport_handshake (receiver, incoming->bytes, incoming->length,
                                    &used, outgoing->bytes + outgoing->length,
                                    sizeof outgoing->bytes - outgoing->length, &written) != 1)
      return 0;
    outgoing->length += written;
    incoming->length -= used;
    memmove (incoming->bytes, incoming->bytes + used, incoming->length);
  }
  return 1;
}

static int
transfer (mulib_sshtransport_t *from, mulib_sshtransport_t *to,
          const void *data, size_t length, frame_t *frame, void *scratch, mulib_ssh_slice_t *payload)
{
  size_t used;
  return mulib_sshtransport_seal (from, data, length, frame->bytes, sizeof frame->bytes,
                                  &frame->length) == 1
    && mulib_sshtransport_open (to, frame->bytes, frame->length, scratch, MULIB_SSH_BODY_MAX,
                                payload, &used) == 1 && used == frame->length;
}

int
main (void)
{
  const unsigned char host_seed[32] = { 1 }, user_seed[32] = { 2 };
  const mulib_ssh_slice_t vc = SLICE ("SSH-2.0-example-client");
  const mulib_ssh_slice_t vs = SLICE ("SSH-2.0-example-server comment");
  const mulib_ssh_slice_t user = SLICE ("example-user");
  const mulib_ssh_slice_t hello = SLICE ("hello over SSH");
  const mulib_ssh_channel_endpoint_t client_ep = { 1, 4096, 1024 }, server_ep = { 2, 4096, 1024 };
  unsigned char host_pin[32], user_pin[32], sid[32];
  unsigned char plaintext[256], scratch[MULIB_SSH_BODY_MAX];
  mulib_sshtransport_t client = { 0 }, server = { 0 };
  mulib_ssh_auth_request_t request;
  mulib_ssh_auth_reply_t reply;
  mulib_ssh_channel_endpoint_t peer;
  mulib_ssh_channel_event_t event;
  mulib_sshchannel_t client_channel, server_channel;
  mulib_ssh_slice_t payload;
  frame_t from_client, from_server, saved;
  size_t length;
  int ok = mulib_ed25519_public (host_pin, host_seed)
    && mulib_ed25519_public (user_pin, user_seed)
    && mulib_sshclient_start (&client, vc, vs, host_pin, from_client.bytes,
                             sizeof from_client.bytes, &from_client.length)
    && mulib_sshserver_start (&server, vc, vs, host_seed, from_server.bytes,
                             sizeof from_server.bytes, &from_server.length)
    && drive (&server, &from_client, &saved) && saved.length == 0
    && drive (&client, &from_server, &from_client)
    && drive (&server, &from_client, &from_server)
    && drive (&client, &from_server, &from_client)
    && drive (&server, &from_client, &from_server)
    && mulib_sshtransport_ready (&client) && mulib_sshtransport_ready (&server)
    && mulib_sshauth_service_write (0, plaintext, sizeof plaintext, &length)
    && transfer (&client, &server, plaintext, length, &from_client, scratch, &payload)
    && mulib_sshauth_service_parse (0, payload.data, payload.length)
    && mulib_sshauth_service_write (1, plaintext, sizeof plaintext, &length)
    && transfer (&server, &client, plaintext, length, &from_server, scratch, &payload)
    && mulib_sshauth_service_parse (1, payload.data, payload.length)
    && mulib_sshclient_public_auth (&client, user, user_seed, plaintext, sizeof plaintext, &length)
    && transfer (&client, &server, plaintext, length, &from_client, scratch, &payload)
    && mulib_sshtransport_session_id (&server, sid)
    && mulib_sshauth_public_verify (payload.data, payload.length, sid, &request)
    && mulib_ssh_slice_is (request.user, "example-user")
    && memcmp (request.public_key.data, user_pin, sizeof user_pin) == 0
    && mulib_sshauth_reply_write (1, (mulib_ssh_slice_t){ NULL, 0 }, 0,
                                 plaintext, sizeof plaintext, &length)
    && transfer (&server, &client, plaintext, length, &from_server, scratch, &payload)
    && mulib_sshauth_reply_parse (payload.data, payload.length, &reply)
    && reply.type == MULIB_SSH_USERAUTH_SUCCESS
    && mulib_sshchannel_open_write (client_ep, plaintext, sizeof plaintext, &length)
    && transfer (&client, &server, plaintext, length, &from_client, scratch, &payload)
    && mulib_sshchannel_open_parse (payload.data, payload.length, &peer)
    && mulib_sshchannel_init (&server_channel, server_ep, peer)
    && mulib_sshchannel_confirm_write (peer.id, server_ep, plaintext, sizeof plaintext, &length)
    && transfer (&server, &client, plaintext, length, &from_server, scratch, &payload)
    && mulib_sshchannel_confirm_parse (payload.data, payload.length, client_ep.id, &peer)
    && mulib_sshchannel_init (&client_channel, client_ep, peer)
    && mulib_sshchannel_data (&client_channel, 0, hello, plaintext, sizeof plaintext, &length)
    && transfer (&client, &server, plaintext, length, &from_client, scratch, &payload)
    && mulib_sshchannel_receive (&server_channel, payload.data, payload.length, &event)
    && event.type == MULIB_SSH_CHANNEL_DATA && mulib_ssh_slice_is (event.data, "hello over SSH");

  mulib_sshtransport_clear (&client);
  mulib_sshtransport_clear (&server);
  mulib_secret_wipe (plaintext, sizeof plaintext);
  mulib_secret_wipe (scratch, sizeof scratch);
  if (!ok) {
    fputs ("SSH kernel example failed\n", stderr);
    return 1;
  }
  puts ("Pinned host, authenticated user, and exchanged SSH channel data.");
  return 0;
}
