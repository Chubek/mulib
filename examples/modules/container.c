#include "../container-stack-common.h"
int main (void)
{
  mulib_container_spec spec;
  mulib_container container;
  assert (!mulib_container_spec_init (&spec, "worker"));
  assert (!mulib_container_spec_arg (&spec, "/bin/worker"));
  assert (!mulib_container_spec_env (&spec, "MODE", "batch"));
  assert (!mulib_container_init (&container, &spec));
  mulib_container_spec_free (&spec);
  assert (!strcmp (mulib_container_spec_getenv (&container.spec, "MODE"), "batch"));
  assert (!mulib_container_transition (&container, MULIB_CONTAINER_RUNNING, 1, 0));
  assert (!mulib_container_transition (&container, MULIB_CONTAINER_EXITED, 2, 0));
  mulib_container_free (&container);
  return 0;
}
