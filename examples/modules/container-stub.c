#include "../container-stack-common.h"
typedef struct endpoint { mulib_containerd_ctrlbridge *bridge; } endpoint;
static int send_bytes (void *ctx, const void *p, size_t n, size_t *used)
{
  int rc = mulib_containerd_ctrlbridge_feed (((endpoint *)ctx)->bridge, p, n, used, 1);
  return rc < 0 ? rc : *used ? 0 : MULIB_CT_AGAIN;
}
static int read_bytes (void *ctx, void *p, size_t n, size_t *used)
{
  int rc = mulib_containerd_ctrlbridge_drain (((endpoint *)ctx)->bridge, p, n, used);
  return rc < 0 ? rc : *used ? 0 : MULIB_CT_AGAIN;
}
int main (void)
{
  mulib_containerd daemon;
  mulib_container_qvmboost config;
  mulib_containerd_accessstub policy;
  mulib_containerd_principal principal = { 1, 0, 0, 1 };
  mulib_containerd_ctrlbridge bridge;
  mulib_container_stub client;
  mulib_conainerd_command command;
  mulib_conainerd_status status;
  endpoint connection;
  mulib_container_stub_transport transport = { send_bytes, read_bytes, &connection };
  int rc;
  assert (!example_container_daemon (&daemon, &config));
  assert (!mulib_containerd_accessstub_init (&policy, 0, 1));
  assert (!mulib_containerd_ctrlbridge_init (&bridge, &daemon, &policy, &principal));
  connection.bridge = &bridge;
  assert (!mulib_container_stub_init (&client, &transport, 1));
  memset (&command, 0, sizeof (command)); strcpy (command.id, "missing");
  command.opcode = MULIB_CONAINERD_STATUS;
  assert (!mulib_container_stub_begin (&client, &command));
  do { rc = mulib_container_stub_pump (&client, 8); } while (rc == MULIB_CT_AGAIN);
  assert (rc == MULIB_CT_DONE && mulib_container_stub_take (&client, &status) == MULIB_CT_ENOENT);
  mulib_container_stub_free (&client); mulib_containerd_ctrlbridge_free (&bridge);
  mulib_containerd_accessstub_free (&policy); assert (!mulib_containerd_free (&daemon, 2));
  return 0;
}
