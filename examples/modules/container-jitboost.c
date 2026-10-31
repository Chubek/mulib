#include "../container-native-common.h"
int main (void)
{
  mulib_mir_flow_instruction_t code[] = {
    { MULIB_MIR_FLOW_CONST, 0, MULIB_MIR_NONE, MULIB_MIR_NONE, MULIB_MIR_NONE, { 7 } },
    { MULIB_MIR_FLOW_RETURN, MULIB_MIR_NONE, 0, MULIB_MIR_NONE, MULIB_MIR_NONE, { 0 } }
  };
  mulib_mir_type_t types[] = { MULIB_MIR_I64 };
  mulib_mir_flow_program_t program = { code, 2, types, 1, { NULL, 0, MULIB_MIR_I64 }, NULL, 0 };
  mulib_container_jitboost config;
  mulib_container_runtime_ops ops = mulib_container_jitboost_ops ();
  mulib_container_spec spec;
  mulib_container container;
  mulib_container_runtime runtime;
  int rc;
  mulib_container_jitboost_init (&config, &program);
  assert (!mulib_container_spec_init (&spec, "native"));
  assert (!mulib_container_init (&container, &spec));
  rc = mulib_container_runtime_init (&runtime, &container, NULL, &ops, &config);
  if (!rc)
    {
      assert (!mulib_container_runtime_start (&runtime, 0));
      assert (mulib_container_runtime_step (&runtime, 8, 1) == MULIB_CT_DONE);
      assert (container.exit_code == 7 && !mulib_container_runtime_close (&runtime, 2));
    }
  else assert (rc == MULIB_CT_ENOTSUP || rc == MULIB_CT_EIO);
  mulib_container_free (&container); mulib_container_spec_free (&spec);
  return 0;
}
