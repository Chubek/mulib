#define MULIB_IMPL_SCRAPE_SCHEMA
#include "mulib-scrape-schema.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  assert (mulib_scrape_schema_string ("abc", 1, 4));
  assert (!mulib_scrape_schema_string ("", 1, 4));
  int64_t integer;
  double real;
  int boolean;
  assert (mulib_scrape_schema_integer ("42", 0, 100, &integer) && integer == 42);
  assert (mulib_scrape_schema_real ("0.5", 0, 1, &real) && real == 0.5);
  assert (mulib_scrape_schema_boolean ("true", &boolean) && boolean == 1);
  return 0;
}
