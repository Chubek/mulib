#include "../container-stack-common.h"
int main (void)
{
  mulib_conainerd_command command, parsed;
  mulib_container_ipc_message message;
  mulib_ct_blob wire = { 0 };
  size_t consumed;
  memset (&command, 0, sizeof (command)); command.opcode = MULIB_CONAINERD_STATUS;
  command.request_id = 10; strcpy (command.id, "worker");
  assert (!mulib_conainerd_ipc_command (&command, &wire));
  assert (!mulib_container_ipc_decode (wire.data, wire.length, 96, &message, &consumed));
  assert (!mulib_conainerd_ipc_parse (&message, &parsed));
  assert (!strcmp (parsed.id, "worker") && parsed.request_id == 10);
  mulib_ct_blob_free (&wire);
  return 0;
}
