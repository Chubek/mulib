#include "../container-stack-common.h"
int main (void)
{
  mulib_container_qvmboost config;
  mulib_container_runtime_ops ops = mulib_container_qvmboost_ops ();
  mulib_container_runtime runtime;
  mulib_container_spec spec;
  mulib_container container;
  example_container_qvm (&config);
  assert (!mulib_container_spec_init (&spec, "runtime"));
  assert (!mulib_container_init (&container, &spec));
  assert (!mulib_container_runtime_init (&runtime, &container, NULL, &ops, &config));
  assert (!mulib_container_runtime_start (&runtime, 1));
  assert (mulib_container_runtime_step (&runtime, 10, 2) == MULIB_CT_DONE);
  assert (container.exit_code == 42);
  assert (!mulib_container_runtime_close (&runtime, 3));
  mulib_container_free (&container); mulib_container_spec_free (&spec);
  return 0;
}
