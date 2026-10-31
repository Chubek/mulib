#include "../container-stack-common.h"
int main (void)
{
  mulib_containerd_accessstub policy;
  mulib_containerd_access_rule rule;
  mulib_containerd_principal principal = { 42, 1000, 1000, 1 };
  mulib_conainerd_command command;
  assert (!mulib_containerd_accessstub_init (&policy, 4, 0));
  memset (&rule, 0, sizeof (rule)); rule.subject = 42; rule.allow = 1;
  rule.operations = 1U << (MULIB_CONAINERD_STATUS - 1);
  assert (!mulib_containerd_accessstub_add (&policy, &rule));
  memset (&command, 0, sizeof (command)); strcpy (command.id, "worker");
  command.opcode = MULIB_CONAINERD_STATUS;
  assert (!mulib_containerd_accessstub_check (&policy, &principal, &command, NULL));
  command.opcode = MULIB_CONAINERD_DELETE;
  assert (mulib_containerd_accessstub_check (&policy, &principal, &command, NULL) == MULIB_CT_EACCESS);
  mulib_containerd_accessstub_free (&policy);
  return 0;
}
