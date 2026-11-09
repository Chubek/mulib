#define MULIB_IMPL_CRAWLQUEUE
#include "mulib-crawlqueue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  mulib_crawlqueue_t q = { 0 };
  assert (mulib_crawlqueue_push (&q, "https://a/") == 1);
  assert (mulib_crawlqueue_push (&q, "https://a/") == 2);
  assert (!strcmp (mulib_crawlqueue_next (&q), "https://a/"));
  mulib_crawlqueue_free (&q);
  return 0;
}
