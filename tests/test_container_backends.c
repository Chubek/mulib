#include "../examples/container-native-common.h"
static void jit_backend (void)
{
  mulib_mir_flow_instruction_t code[2];
  mulib_mir_type_t types[] = { MULIB_MIR_I64 };
  mulib_mir_flow_program_t program;
  mulib_container_jitboost config;
  mulib_container_runtime_ops ops;
  mulib_container_runtime runtime;
  mulib_container_spec spec;
  mulib_container container;
  mulib_mir_status_t status;
  mulib_mir_value_t value;
  size_t steps;
  int rc;
  memset (code, 0, sizeof (code)); memset (&program, 0, sizeof (program));
  code[0].opcode = MULIB_MIR_FLOW_CONST; code[0].destination = 0;
  code[0].a = code[0].b = code[0].target = MULIB_MIR_NONE; code[0].immediate.integer = 37;
  code[1].opcode = MULIB_MIR_FLOW_RETURN; code[1].a = 0;
  code[1].destination = code[1].b = code[1].target = MULIB_MIR_NONE;
  program.code = code; program.count = 2; program.register_types = types;
  program.register_count = 1; program.signature.result = MULIB_MIR_I64;
  mulib_container_jitboost_init (&config, &program);
  assert (!mulib_container_spec_init (&spec, "jit"));
  assert (!mulib_container_init (&container, &spec));
  ops = mulib_container_jitboost_ops ();
  rc = mulib_container_runtime_init (&runtime, &container, NULL, &ops, &config);
  if (!mulib_jit_supported ()) assert (rc == MULIB_CT_ENOTSUP);
  else
    {
      assert (!rc);
      assert (!mulib_container_runtime_start (&runtime, 1));
      assert (mulib_container_runtime_step (&runtime, 1, 2) == MULIB_CT_AGAIN);
      assert (mulib_container_runtime_step (&runtime, 1, 3) == MULIB_CT_DONE);
      assert (container.exit_code == 37);
      assert (!mulib_container_jitboost_inspect (&runtime, &status, &steps, &value));
      assert (status == MULIB_MIR_FINISHED && steps == 2 && value.integer == 37);
      assert (!mulib_container_runtime_start (&runtime, 4));
      assert (!mulib_container_runtime_signal (&runtime, 15));
      assert (mulib_container_runtime_step (&runtime, 1, 5) == MULIB_CT_DONE && container.exit_code == 143);
      assert (!mulib_container_runtime_close (&runtime, 6));
    }
  mulib_container_free (&container); mulib_container_spec_free (&spec);
}
static void oix_compilation (void)
{
  mulib_rootfs_t root;
  mulib_oix_config_t config;
  mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
  mulib_oix_info_t info;
  mulib_container_oixboost adapter;
  mulib_containerboost_backend backend;
  assert (!mulib_rootfs_init (&root, 8, 1024));
  assert (!mulib_rootfs_put (&root, "/hello", "world", 5, 0));
  mulib_oix_config_init (&config);
  assert (!mulib_container_oixboost_compile (&root, &config, &image, NULL));
  assert (mulib_oix_validate (image.data, image.length, &info, NULL) == MULIB_OIX_OK && info.files == 1);
  mulib_container_oixboost_init (&adapter, image.data, image.length);
  assert (!mulib_container_oixboost_backend (&adapter, "oix", 100, &backend));
  assert (backend.kind == MULIB_CONTAINERBOOST_OIX);
  image.data[0] ^= 1;
  assert (mulib_container_oixboost_backend (&adapter, "oix", 100, &backend) == MULIB_CT_EFORMAT);
  mulib_oix_image_free (&image); mulib_rootfs_free (&root);
}
int main (void)
{
  mulib_container_kvmboost config;
  mulib_containerboost_backend backend;
  mulib_container_kvmboost_init (&config);
  assert (mulib_container_kvmboost_backend (&config, "kvm", 1, &backend) == MULIB_CT_EINVAL);
  assert (mulib_container_kvmboost_probe ("/mulib-nonexistent-kvm-device") < 0);
  jit_backend (); oix_compilation ();
  return 0;
}
