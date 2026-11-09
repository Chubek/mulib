#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static size_t fail_at, calls, live;
static void *checked_malloc (size_t n)
{ void *p; if (++calls == fail_at) return NULL; p = malloc (n); if (p) ++live; return p; }
static void *checked_realloc (void *old, size_t n)
{ void *p; int had = old != NULL; assert (n); if (++calls == fail_at) return NULL; p = realloc (old,n); if (p && !had) ++live; return p; }
static void checked_free (void *p) { if (p) { assert (live); --live; } free (p); }
#define malloc checked_malloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_OBJFILEBUILDER
#define MULIB_IMPL_ELFBUILDER
#define MULIB_IMPL_COFFBUILDER
#define MULIB_IMPL_PEBUILDER
#define MULIB_IMPL_MACHOBUILDER
#include "mulib-elfbuilder.h"
#include "mulib-coffbuilder.h"
#include "mulib-pebuilder.h"
#include "mulib-machobuilder.h"
#undef malloc
#undef realloc
#undef free
int main (void)
{
  mulib_objfilebuilder_t builder = {0};
  const unsigned char ret[] = {0xc0,3,0x5f,0xd6};
  size_t action, k, baseline;
  assert (mulib_objfilebuilder_add (&builder,".text",ret,sizeof (ret),16,6));
  baseline = live;
  for (action = 0; action < 6; ++action) {
    int success = 0;
    for (k = 1; k < 100; ++k) {
      mulib_buffer_t out = {0}; unsigned char *original;
      fail_at = 0; assert (mulib_buffer_append (&out,"prefix",6)); original = out.data;
      calls = 0; fail_at = k;
      switch (action) {
        case 0: success = mulib_elfbuilder_finish_abi (&builder,MULIB_ELFBUILDER_ANDROID_ARMEABI_V7A,&out); break;
        case 1: success = mulib_elfbuilder_finish_abi (&builder,MULIB_ELFBUILDER_ANDROID_ARM64_V8A,&out); break;
        case 2: success = mulib_coffbuilder_finish_abi (&builder,MULIB_COFFBUILDER_MS_ARM64,&out); break;
        case 3: success = mulib_pebuilder_finish_abi (&builder,MULIB_PEBUILDER_WINDOWS_ARM64,&out); break;
        case 4: success = mulib_pebuilder_text_abi (&out,ret,sizeof (ret),0,MULIB_PEBUILDER_UEFI_ARM64); break;
        default: success = mulib_machobuilder_finish_abi (&builder,MULIB_MACHOBUILDER_IOS_ARM64,0xe0000,0,&out); break;
      }
      assert (!memcmp (out.data,"prefix",6));
      if (!success) assert (out.length == 6 && out.data == original);
      mulib_buffer_free (&out); assert (live == baseline); if (success) break;
    }
    assert (success && k > 2);
  }
  mulib_objfilebuilder_free (&builder); assert (!live); return 0;
}
