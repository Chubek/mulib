#include "alloc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int hits;

static int mode(const char *wanted)
{
  const char *actual = getenv("ELVI_ALLOC_TEST");
  return actual && !strcmp(actual, wanted);
}

void *elvi_test_hit_alloc(size_t n)
{
  assert(hits == 0);
  if (mode("native"))
    return NULL;
  void *p = malloc(n);
  if (p)
    ++hits;
  return p;
}

void elvi_test_hit_free(void *p)
{
  assert(hits == 1 && p);
  --hits;
  free(p);
}

void *elvi_test_text_alloc(size_t n)
{
  assert(hits == 0); /* No native resources may survive snapshot allocation. */
  return mode("text") ? NULL : malloc(n);
}
