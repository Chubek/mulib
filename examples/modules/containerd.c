#include "../container-stack-common.h"
int main (void)
{
  mulib_containerd daemon;
  mulib_container_qvmboost config;
  mulib_ct_blob image = { 0 };
  mulib_conainerd_command command;
  mulib_conainerd_status status;
  assert (!example_container_image (&image));
  assert (!example_container_daemon (&daemon, &config));
  assert (!mulib_containerd_create (&daemon, image.data, image.length, "worker", NULL, 0, 0));
  memset (&command, 0, sizeof (command)); strcpy (command.id, "worker");
  command.opcode = MULIB_CONAINERD_START;
  assert (!mulib_containerd_dispatch (&daemon, &command, &status));
  assert (!mulib_containerd_tick (&daemon, 4, 100, 1, NULL));
  command.opcode = MULIB_CONAINERD_STATUS;
  assert (!mulib_containerd_dispatch (&daemon, &command, &status));
  assert (status.state == MULIB_CONTAINER_EXITED && status.exit_code == 42);
  assert (!mulib_containerd_free (&daemon, 2)); mulib_ct_blob_free (&image);
  return 0;
}
