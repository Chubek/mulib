#define MULIB_IMPL_ACL
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4
#define MULIB_IMPL_PARSEC
#include "mulib-acl.h"
#include <assert.h>
int
main (void)
{
  const mulib_acl_rule_t rules[] = { { 0x7f000000, 8, 1 }, { 0, 0, 0 } };
  assert (mulib_acl_check (rules, 2, 0x7f000001, 0));
  assert (!mulib_acl_check (rules, 2, 0x0a000001, 0));
  return 0;
}
