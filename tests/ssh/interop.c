/* Standalone test peer; deliberately accepts only one fixed test command. */
#include "mulib-sshchannel.h"
#include "mulib-sshclient.h"
#include "mulib-sshserver.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SSH_SLICE(s) { (const unsigned char *)(s), sizeof (s) - 1 }
#define REQUIRE(condition) do { if (!(condition)) {                         \
  fprintf (stderr, "SSH fixture: line %d: %s\n", __LINE__, #condition);      \
  return 0;                                                                \
} } while (0)

static const unsigned char host_seed[32] = { 0x27 }, user_seed[32] = { 0x36 };
static const mulib_ssh_slice_t version = SSH_SLICE ("SSH-2.0-MuLIB-fixture interop");
static const mulib_ssh_slice_t ed25519 = SSH_SLICE ("publickey");
static const mulib_ssh_slice_t output_text = SSH_SLICE ("mulib-ssh-interop");
static const mulib_ssh_slice_t error_text = SSH_SLICE ("mulib-ssh-stderr");
static const mulib_ssh_slice_t command = SSH_SLICE ("printf mulib-ssh-interop; printf mulib-ssh-stderr >&2");

typedef struct {
  mulib_sshtransport_t transport;
  unsigned char incoming[MULIB_SSH_FRAME_MAX], outgoing[MULIB_SSH_FRAME_MAX];
  unsigned char scratch[MULIB_SSH_BODY_MAX];
  size_t length;
} peer_t;

static int
send_bytes (const void *data, size_t n)
{
  const unsigned char *p = data;
  while (n) {
    size_t chunk = n < 127 ? n : 127;
    ssize_t written = write (STDOUT_FILENO, p, chunk);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) return 0;
    n -= (size_t)written;
    p += written;
  }
  return 1;
}

static int
fill (peer_t *p)
{
  size_t cap = sizeof p->incoming - p->length;
  if (!cap) return 0;
  ssize_t n;
  do n = read (STDIN_FILENO, p->incoming + p->length, cap < 137 ? cap : 137);
  while (n < 0 && errno == EINTR);
  if (n <= 0) return 0;
  p->length += (size_t)n;
  return 1;
}

static void
consume (peer_t *p, size_t n)
{
  p->length -= n;
  memmove (p->incoming, p->incoming + n, p->length);
}

static int
handshake (peer_t *p, int client, const unsigned char material[32])
{
  unsigned char remote[MULIB_SSH_IDENT_MAX];
  size_t n, used, written;
  REQUIRE (send_bytes (version.data, version.length) && send_bytes ("\r\n", 2));
  for (;;) {
    const unsigned char *lf = memchr (p->incoming, '\n', p->length);
    if (lf) {
      n = (size_t)(lf - p->incoming);
      REQUIRE (n >= 1 && n <= sizeof remote + 1 && lf[-1] == '\r');
      memcpy (remote, p->incoming, n - 1);
      consume (p, n + 1);
      break;
    }
    REQUIRE (p->length < sizeof remote + 2 && fill (p));
  }
  const mulib_ssh_slice_t remote_version = { remote, n - 1 };
  REQUIRE (mulib_sshtransport_start (&p->transport, client ? MULIB_SSH_CLIENT : MULIB_SSH_SERVER,
                                     client ? version : remote_version,
                                     client ? remote_version : version, material,
                                     p->outgoing, sizeof p->outgoing, &written) == 1);
  REQUIRE (send_bytes (p->outgoing, written));
  while (!mulib_sshtransport_ready (&p->transport)) {
    int status = mulib_sshtransport_handshake (&p->transport, p->incoming, p->length,
                                               &used, p->outgoing, sizeof p->outgoing, &written);
    REQUIRE (status >= 0);
    if (status == 0) {
      REQUIRE (fill (p));
    } else {
      consume (p, used);
      REQUIRE (send_bytes (p->outgoing, written));
    }
  }
  return 1;
}

static int
send_payload (peer_t *p, const void *data, size_t n)
{
  size_t written;
  return mulib_sshtransport_seal (&p->transport, data, n, p->outgoing,
                                 sizeof p->outgoing, &written) == 1
    && send_bytes (p->outgoing, written);
}

static int
step (peer_t *p, mulib_ssh_slice_t *payload)
{
  for (;;) {
    size_t used, written;
    int status = mulib_sshtransport_receive (&p->transport, p->incoming, p->length,
                                             p->scratch, sizeof p->scratch, payload, &used,
                                             p->outgoing, sizeof p->outgoing, &written);
    if (status < 0) return 0;
    if (!status) {
      if (!fill (p)) return 0;
      continue;
    }
    consume (p, used);
    return send_bytes (p->outgoing, written);
  }
}

static int
discard_global (peer_t *p, mulib_ssh_slice_t payload)
{
  /* SSH_MSG_GLOBAL_REQUEST: not supported by fixture. */
  mulib_ssh_reader_t r = { payload.data + 1, payload.length - 1 };
  mulib_ssh_slice_t name;
  unsigned char wants_reply, failure = 82;
  return mulib_ssh_read_string (&r, &name) && mulib_ssh_read_u8 (&r, &wants_reply)
    && (!wants_reply || send_payload (p, &failure, 1));
}

static int
receive (peer_t *p, mulib_ssh_slice_t *payload)
{
  while (step (p, payload)) {
    if (!payload->length) continue;
    if (payload->data[0] != 80) return 1;
    REQUIRE (discard_global (p, *payload));
  }
  return 0;
}

static int
rekey (peer_t *p)
{
  size_t written;
  unsigned char before[32], after[32];
  mulib_ssh_slice_t payload;
  REQUIRE (mulib_sshtransport_session_id (&p->transport, before));
  REQUIRE (mulib_sshtransport_rekey (&p->transport, p->outgoing,
                                    sizeof p->outgoing, &written) == 1);
  REQUIRE (send_bytes (p->outgoing, written));
  while (!mulib_sshtransport_ready (&p->transport)) {
    REQUIRE (step (p, &payload));
    REQUIRE (!payload.length || (payload.data[0] == 80 && discard_global (p, payload)));
  }
  return mulib_sshtransport_session_id (&p->transport, after) && !memcmp (before, after, sizeof before);
}

static int
append_matches (mulib_ssh_slice_t expected, size_t *matched, mulib_ssh_slice_t got)
{
  if (got.length > expected.length - *matched
      || memcmp (expected.data + *matched, got.data, got.length)) return 0;
  *matched += got.length;
  return 1;
}

static int
client (peer_t *p, mulib_ssh_slice_t user, const unsigned char pin[32])
{
  const mulib_ssh_slice_t exec = SSH_SLICE ("exec");
  const mulib_ssh_channel_endpoint_t local = { 7, 4096, 4096 };
  mulib_ssh_channel_endpoint_t remote;
  mulib_sshchannel_t channel;
  mulib_ssh_channel_event_t event;
  mulib_ssh_slice_t payload;
  unsigned char buffer[2048], body[128];
  mulib_ssh_writer_t writer = { body, sizeof body };
  size_t n, stdout_matched = 0, stderr_matched = 0;
  int accepted = 0, exit_status = 0;
  REQUIRE (handshake (p, 1, pin));
  REQUIRE (mulib_sshauth_service_write (0, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  REQUIRE (receive (p, &payload) && mulib_sshauth_service_parse (1, payload.data, payload.length));
  REQUIRE (mulib_sshclient_public_auth (&p->transport, user, user_seed, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  REQUIRE (receive (p, &payload) && payload.length == 1
           && payload.data[0] == MULIB_SSH_USERAUTH_SUCCESS);
  REQUIRE (mulib_sshchannel_open_write (local, buffer, sizeof buffer, &n) && send_payload (p, buffer, n));
  REQUIRE (receive (p, &payload)
           && mulib_sshchannel_confirm_parse (payload.data, payload.length, local.id, &remote));
  REQUIRE (rekey (p));
  REQUIRE (mulib_sshchannel_init (&channel, local, remote));
  REQUIRE (mulib_ssh_write_string (&writer, command.data, command.length));
  mulib_ssh_slice_t request = { body, sizeof body - writer.left };
  REQUIRE (mulib_sshchannel_request (&channel, exec, 1, request, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  while (receive (p, &payload)) {
    REQUIRE (mulib_sshchannel_receive (&channel, payload.data, payload.length, &event));
    switch (event.type) {
      case MULIB_SSH_CHANNEL_SUCCESS:
        accepted = 1;
        break;
      case MULIB_SSH_CHANNEL_DATA:
        REQUIRE (append_matches (output_text, &stdout_matched, event.data));
        break;
      case MULIB_SSH_CHANNEL_EXTENDED_DATA:
        REQUIRE (event.value == 1 && append_matches (error_text, &stderr_matched, event.data));
        break;
      case MULIB_SSH_CHANNEL_REQUEST:
        REQUIRE (mulib_ssh_slice_is (event.name, "exit-status") && !event.want_reply
                 && event.data.length == 4 && mulib_endian_load_be32 (event.data.data) == 0);
        exit_status = 1;
        break;
      case MULIB_SSH_CHANNEL_CLOSE:
        REQUIRE (mulib_sshchannel_control (&channel, MULIB_SSH_CHANNEL_CLOSE, buffer, sizeof buffer, &n)
                 && send_payload (p, buffer, n));
        REQUIRE (accepted && exit_status && stdout_matched == output_text.length
                 && stderr_matched == error_text.length);
        return 1;
      case MULIB_SSH_CHANNEL_EOF:
      case MULIB_SSH_CHANNEL_WINDOW_ADJUST:
        break;
      default:
        REQUIRE (0);
    }
  }
  REQUIRE (0);
}

static int
server (peer_t *p, mulib_ssh_slice_t user, const unsigned char key[32], int reject)
{
  const mulib_ssh_slice_t exit_name = SSH_SLICE ("exit-status");
  const unsigned char status[4] = { 0 };
  const mulib_ssh_slice_t exit_body = { status, sizeof status };
  const mulib_ssh_channel_endpoint_t local = { 9, 4096, 4096 };
  mulib_ssh_channel_endpoint_t remote;
  mulib_sshchannel_t channel;
  mulib_ssh_channel_event_t event;
  mulib_ssh_auth_request_t auth;
  mulib_ssh_slice_t payload, requested_command;
  unsigned char buffer[2048], sid[32];
  size_t n;
  int authenticated = 0;
  REQUIRE (handshake (p, 0, host_seed));
  REQUIRE (receive (p, &payload) && mulib_sshauth_service_parse (0, payload.data, payload.length));
  REQUIRE (mulib_sshauth_service_write (1, buffer, sizeof buffer, &n) && send_payload (p, buffer, n));
  REQUIRE (mulib_sshtransport_session_id (&p->transport, sid));
  for (unsigned attempt = 0; attempt < 6 && !authenticated; ++attempt) {
    REQUIRE (receive (p, &payload));
    int authorized = mulib_sshauth_parse (payload.data, payload.length, &auth) == 1
      && auth.method == MULIB_SSH_AUTH_ED25519 && auth.user.length == user.length
      && !memcmp (auth.user.data, user.data, user.length)
      && auth.public_key.length == 32 && !memcmp (auth.public_key.data, key, 32);
    if (authorized && reject) {
      REQUIRE (mulib_sshauth_reply_write (0, ed25519, 0, buffer, sizeof buffer, &n)
               && send_payload (p, buffer, n));
      /* Give the peer time to process the rejection before closing. */
      return !receive (p, &payload);
    }
    if (authorized && !auth.signature.length) {
      REQUIRE (mulib_sshauth_pk_ok_write (key, buffer, sizeof buffer, &n) && send_payload (p, buffer, n));
      continue;
    }
    authenticated = authorized && mulib_sshauth_public_verify (payload.data, payload.length, sid, &auth);
    REQUIRE (mulib_sshauth_reply_write (authenticated, ed25519, 0, buffer, sizeof buffer, &n)
             && send_payload (p, buffer, n));
  }
  REQUIRE (authenticated && !reject);
  REQUIRE (receive (p, &payload) && mulib_sshchannel_open_parse (payload.data, payload.length, &remote));
  REQUIRE (rekey (p));
  REQUIRE (mulib_sshchannel_init (&channel, local, remote)
           && mulib_sshchannel_confirm_write (remote.id, local, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  for (;;) {
    REQUIRE (receive (p, &payload)
             && mulib_sshchannel_receive (&channel, payload.data, payload.length, &event));
    if (event.type == MULIB_SSH_CHANNEL_REQUEST && mulib_ssh_slice_is (event.name, "exec")) break;
    if (event.type == MULIB_SSH_CHANNEL_REQUEST && event.want_reply)
      REQUIRE (mulib_sshchannel_control (&channel, MULIB_SSH_CHANNEL_FAILURE, buffer, sizeof buffer, &n)
               && send_payload (p, buffer, n));
  }
  mulib_ssh_reader_t reader = { event.data.data, event.data.length };
  REQUIRE (mulib_ssh_read_string (&reader, &requested_command) && !reader.left
           && requested_command.length == command.length
           && !memcmp (requested_command.data, command.data, command.length));
  if (event.want_reply)
    REQUIRE (mulib_sshchannel_control (&channel, MULIB_SSH_CHANNEL_SUCCESS, buffer, sizeof buffer, &n)
             && send_payload (p, buffer, n));
  REQUIRE (mulib_sshchannel_data (&channel, 0, output_text, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  REQUIRE (mulib_sshchannel_data (&channel, 1, error_text, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  REQUIRE (mulib_sshchannel_request (&channel, exit_name, 0, exit_body, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  REQUIRE (mulib_sshchannel_control (&channel, MULIB_SSH_CHANNEL_EOF, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  REQUIRE (mulib_sshchannel_control (&channel, MULIB_SSH_CHANNEL_CLOSE, buffer, sizeof buffer, &n)
           && send_payload (p, buffer, n));
  do {
    REQUIRE (receive (p, &payload)
             && mulib_sshchannel_receive (&channel, payload.data, payload.length, &event));
  } while (event.type != MULIB_SSH_CHANNEL_CLOSE);
  return 1;
}

static int
unhex (const char *hex, unsigned char out[32])
{
  const char *digits = "0123456789abcdef";
  if (strlen (hex) != 64) return 0;
  for (size_t i = 0; i < 32; ++i) {
    const char *hi = strchr (digits, hex[2 * i]), *lo = strchr (digits, hex[2 * i + 1]);
    if (!hi || !lo) return 0;
    out[i] = (unsigned char)((hi - digits) * 16 + (lo - digits));
  }
  return 1;
}

int
main (int argc, char **argv)
{
  unsigned char key[32];
  if (argc == 2 && (!strcmp (argv[1], "host-key") || !strcmp (argv[1], "user-key"))) {
    if (!mulib_ed25519_public (key, argv[1][0] == 'h' ? host_seed : user_seed)) return 1;
    for (size_t i = 0; i < sizeof key; ++i) printf ("%02x", (unsigned)key[i]);
    puts ("");
    return 0;
  }
  if (argc != 4 || !unhex (argv[3], key)) return 2;
  const mulib_ssh_slice_t user = { (const unsigned char *)argv[2], strlen (argv[2]) };
  peer_t *p = calloc (1, sizeof *p);
  if (!p) return 1;
  int ok = !strcmp (argv[1], "client") ? client (p, user, key)
    : (!strcmp (argv[1], "server") || !strcmp (argv[1], "reject"))
      && server (p, user, key, argv[1][0] == 'r');
  mulib_sshtransport_clear (&p->transport);
  mulib_secret_wipe (p->scratch, sizeof p->scratch);
  free (p);
  return ok ? 0 : 1;
}
