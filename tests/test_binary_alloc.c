/* Binary writer/builder allocation bounds and failure-atomic publication. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static size_t allocations;
static int fail_allocation;
static void *test_malloc (size_t n)
{ ++allocations; return fail_allocation ? NULL : malloc (n); }
static void *test_calloc (size_t n, size_t width)
{ ++allocations; return fail_allocation ? NULL : calloc (n, width); }
static void *test_realloc (void *p, size_t n)
{ ++allocations; return fail_allocation ? NULL : realloc (p, n); }
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BINFILESCHEMA
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BINFILEBUILDER
#include "mulib-binfilewriter.h"
#include "mulib-binfilebuilder.h"

int main (void)
{
  mulib_buffer_t b = { 0 };
  mulib_binfileschema_field_t field = { "value", 0, 1 };
  mulib_binfileschema_t schema = { &field, 1, 4096 };
  uint64_t value = 256;
  size_t before, i;
  unsigned char *saved;
  /* Use the injected malloc too, and provide capacity without scratch storage. */
  b.data = (unsigned char *)test_malloc (64); assert (b.data);
  b.capacity = 64; b.length = 3; memcpy (b.data, "abc", 3);
  before = allocations; fail_allocation = 1;
  assert (mulib_binfilewriter_align (&b, 32));
  assert (allocations == before && b.length == 32 && !memcmp (b.data, "abc", 3));
  for (i = 3; i < b.length; ++i) assert (!b.data[i]);
  saved = b.data;
  assert (!mulib_binfilewriter_align (&b, 128));
  assert (b.data == saved && b.length == 32 && b.capacity == 64);
  before = allocations;
  assert (!mulib_binfilebuilder_record (&b, &schema, &value, 1));
  assert (allocations == before); /* Reject a non-fitting value before allocating. */
  value = 42;
  assert (!mulib_binfilebuilder_record (&b, &schema, &value, 1));
  assert (b.data == saved && b.length == 32);
  fail_allocation = 0; before = allocations;
  assert (mulib_binfilewriter_align (&b, 128));
  assert (allocations == before + 1 && b.length == 128);
  assert (mulib_binfilebuilder_record (&b, &schema, &value, 1));
  assert (b.length == 4224 && b.data[128] == 42);
  for (i = 129; i < b.length; ++i) assert (!b.data[i]);
  assert (!mulib_binfilewriter_align (&b, 3));
  {
    mulib_buffer_t invalid = { b.data, b.capacity + 1, b.capacity };
    unsigned char first = b.data[0];
    assert (!mulib_binfilewriter_align (&invalid, 1));
    assert (!mulib_binfilewriter_patch (&invalid, 0, 1, 0));
    assert (!mulib_binfilebuilder_record (&invalid, &schema, &value, 1));
    assert (b.data[0] == first);
  }
  mulib_buffer_free (&b);
  return 0;
}
