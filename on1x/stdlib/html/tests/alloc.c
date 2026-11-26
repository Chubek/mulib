#include "alloc.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

static int64_t budget = -1, live;

void on1x_html_test_budget_v1(int64_t n)
{
  budget = n;
}

int64_t on1x_html_test_live_v1(void)
{
  return live;
}

static int allow(void)
{
  if (budget < 0) return 1;
  if (!budget) return 0;
  --budget;
  return 1;
}

void *on1x_html_test_malloc(size_t n)
{
  void *p = allow() ? malloc(n) : NULL;
  if (p) ++live;
  return p;
}

void *on1x_html_test_calloc(size_t count, size_t size)
{
  void *p = allow() ? calloc(count, size) : NULL;
  if (p) ++live;
  return p;
}

void *on1x_html_test_realloc(void *p, size_t n)
{
  int fresh = p == NULL;
  assert(n);
  p = allow() ? realloc(p, n) : NULL;
  if (p && fresh) ++live;
  return p;
}

void on1x_html_test_free(void *p)
{
  if (p) {
    assert(live > 0);
    --live;
  }
  free(p);
}

/* The real On1x pool exits on failure. Neither markup scratch allocations nor
 * partially constructed native trees may remain when it crosses that path. */
void *on1x_html_test_fail_data_malloc(size_t n)
{
  (void)n;
  assert(live == 0);
  return NULL;
}
