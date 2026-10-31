#include "../container-stack-common.h"
int main (void)
{
  mulib_containerboost registry;
  mulib_containerboost_backend backend;
  mulib_container_qvmboost config;
  example_container_qvm (&config);
  assert (!mulib_containerboost_init (&registry, 4));
  assert (!mulib_container_qvmboost_backend (&config, "quota", 10, &backend));
  assert (!mulib_containerboost_register (&registry, &backend));
  assert (mulib_containerboost_select (&registry, MULIB_IMGFORMAT_PORTABLE,
      MULIB_CONTAINER_CAP_BUDGET, NULL) != NULL);
  assert (!mulib_containerboost_remove (&registry, "quota"));
  mulib_containerboost_free (&registry);
  return 0;
}
