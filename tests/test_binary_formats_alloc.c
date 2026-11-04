/* Fail each allocation in turn; use real libc for the allocation wrappers. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
static size_t fail_at = SIZE_MAX, allocation_count;
static void *binary_malloc (size_t n)
{ return allocation_count++ == fail_at ? NULL : malloc (n); }
static void *binary_realloc (void *p, size_t n)
{ return allocation_count++ == fail_at ? NULL : realloc (p, n); }
#define malloc binary_malloc
#define realloc binary_realloc
#include "../examples/binary-formats-common.h"
#undef malloc
#undef realloc

static void test_add (void)
{
  size_t i;
  for (i = 0; i < 16; ++i) {
    mulib_machobuilder_t b = { 0 };
    int ok;
    fail_at = i; allocation_count = 0;
    ok = mulib_machobuilder_add (&b, "__data", "abc", 3, 8, 3);
    fail_at = SIZE_MAX;
    if (!ok) assert (!b.count && !b.sections);
    mulib_machobuilder_free (&b);
    if (ok) break;
  }
  assert (i < 16 && i >= 3);
}
static void test_finish_and_dump (void)
{
  mulib_coffbuilder_t builder = { 0 };
  mulib_buffer_t object = { 0 };
  mulib_terminfo_t info = { 0 };
  mulib_termstyle_t style;
  mulib_termstyle_spec_t spec = { MULIB_TERMSTYLE_BOLD, 2, -1 };
  size_t i;
  assert (mulib_coffbuilder_add (&builder, ".text", "\xc3", 1, 16, 6));
  assert (mulib_coffbuilder_finish (&builder, 0x8664, &object));
  for (i = 0; i < 64; ++i) {
    mulib_strbuf_t out = { 0 };
    int ok;
    assert (mulib_strbuf_append (&out, "unchanged"));
    fail_at = i; allocation_count = 0;
    ok = mulib_coff_objdump (object.data, object.length, NULL, &out);
    fail_at = SIZE_MAX;
    if (!ok) assert (!strcmp (out.data, "unchanged"));
    mulib_strbuf_free (&out);
    if (ok) break;
  }
  assert (i < 64 && i > 0);
  for (i = 0; i < 64; ++i) {
    mulib_buffer_t out = { 0 };
    int ok;
    assert (mulib_buffer_append (&out, "unchanged", 9));
    fail_at = i; allocation_count = 0;
    ok = mulib_coffbuilder_finish (&builder, 0x8664, &out);
    fail_at = SIZE_MAX;
    if (!ok) assert (out.length == 9 && !memcmp (out.data, "unchanged", 9));
    mulib_buffer_free (&out);
    if (ok) break;
  }
  assert (i < 64 && i > 0);
  mulib_terminfo_ansi (&info); mulib_termstyle_init (&style, &info, 1);
  for (i = 0; i < 64; ++i) {
    mulib_strbuf_t out = { 0 };
    int ok;
    assert (mulib_strbuf_append (&out, "unchanged"));
    fail_at = i; allocation_count = 0;
    ok = mulib_termstyle_text (&style, &spec, "text", 4, &out);
    fail_at = SIZE_MAX;
    if (!ok) assert (!strcmp (out.data, "unchanged"));
    mulib_strbuf_free (&out);
    if (ok) break;
  }
  assert (i < 64 && i > 0);
  mulib_coffbuilder_free (&builder); mulib_buffer_free (&object); mulib_terminfo_free (&info);
}
int main (void)
{
  mulib_ct_blob image = { 0 }, out = { 0 };
  example_binary_image (&image);
  assert (!mulib_ct_blob_append (&out, "old", 3, 65536));
  allocation_count = 0; fail_at = 0;
  assert (mulib_container_packpe_object (image.data, image.length, 0x8664, &out, 65536) == MULIB_CT_ENOMEM);
  fail_at = SIZE_MAX; assert (out.length == 3 && !memcmp (out.data, "old", 3));
  test_add (); test_finish_and_dump ();
  mulib_ct_blob_free (&image); mulib_ct_blob_free (&out); return 0;
}
