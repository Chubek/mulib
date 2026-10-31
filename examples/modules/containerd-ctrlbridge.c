#include "../container-stack-common.h"
int main (void)
{
  mulib_containerd daemon;
  mulib_container_qvmboost config;
  mulib_containerd_accessstub policy;
  mulib_containerd_principal principal = { 1, 0, 0, 1 };
  mulib_containerd_ctrlbridge bridge;
  mulib_conainerd_command command;
  mulib_container_ipc_message response;
  mulib_ct_blob request = { 0 };
  unsigned char reply[80];
  size_t count, used;
  assert (!example_container_daemon (&daemon, &config));
  assert (!mulib_containerd_accessstub_init (&policy, 0, 1));
  assert (!mulib_containerd_ctrlbridge_init (&bridge, &daemon, &policy, &principal));
  memset (&command, 0, sizeof (command)); strcpy (command.id, "absent");
  command.opcode = MULIB_CONAINERD_STATUS; command.request_id = 1;
  assert (!mulib_conainerd_ipc_command (&command, &request));
  assert (mulib_containerd_ctrlbridge_feed (&bridge, request.data, request.length, &used, 1) == MULIB_CT_DONE);
  assert (mulib_containerd_ctrlbridge_drain (&bridge, reply, sizeof (reply), &count) == MULIB_CT_DONE);
  assert (!mulib_container_ipc_decode (reply, count, 48, &response, &used) && response.status == MULIB_CT_ENOENT);
  mulib_containerd_ctrlbridge_free (&bridge); mulib_containerd_accessstub_free (&policy);
  assert (!mulib_containerd_free (&daemon, 2)); mulib_ct_blob_free (&request);
  return 0;
}
