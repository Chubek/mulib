#define MULIB_IMPL_SCRAPE_RULE
#include "mulib-scrape-rule.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_scrape_rule_t r = { 0 };
  assert (mulib_scrape_rule_init (&r, "h1", NULL, 0));
  mulib_scrape_rule_free (&r);
  return 0;
}
