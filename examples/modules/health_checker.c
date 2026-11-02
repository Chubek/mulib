#define MULIB_IMPL_HEALTH_CHECKER
#define MULIB_IMPL_UPSTREAM
#include "mulib-health_checker.h"
#include <assert.h>
int
main (void)
{
  mulib_upstream_t server = { "localhost", 80, 0, 10, 1 };
  mulib_health_checker_t check = { 0, 0, 2, 2 };
  assert (mulib_health_checker_record (&check, &server, 0) && server.healthy);
  assert (mulib_health_checker_record (&check, &server, 0) && !server.healthy);
  return 0;
}
