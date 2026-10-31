#include "../container-stack-common.h"
int main (void)
{
  mulib_containerd daemon;
  mulib_container_qvmboost config;
  mulib_ct_blob image = { 0 };
  mulib_conainerd_command command;
  mulib_conainerd_status status;
  mulib_containterpool_report report;
  assert (!example_container_image (&image));
  assert (!example_container_daemon (&daemon, &config));
  assert (!mulib_containerd_create (&daemon, image.data, image.length, "quota", "qvm", 0, 0));
  memset (&command, 0, sizeof (command)); strcpy (command.id, "quota");
  command.opcode = MULIB_CONAINERD_START;
  assert (!mulib_containerd_dispatch (&daemon, &command, &status));
  assert (!mulib_containerd_tick (&daemon, 4, 10, 1, &report) && report.exited == 1);
  assert (!mulib_containerd_free (&daemon, 2)); mulib_ct_blob_free (&image);
  return 0;
}
