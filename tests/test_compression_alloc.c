#include <assert.h>
#include <stdlib.h>
#include <string.h>
static size_t calls,fail_at,live;
static void *test_malloc (size_t n)
{ void *p; if (++calls==fail_at) return NULL; p=malloc (n); if (p) ++live; return p; }
static void *test_calloc (size_t n,size_t size)
{ void *p; if (++calls==fail_at) return NULL; p=calloc (n,size); if (p) ++live; return p; }
static void *test_realloc (void *p,size_t n)
{ void *next; int fresh=p==NULL; if (++calls==fail_at) return NULL; next=realloc (p,n); if (next && fresh) ++live; return next; }
static void test_free (void *p)
{ if (p) { assert (live); --live; free (p); } }
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#include "compression_test_setup.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
int main (void)
{
  unsigned char data[2048]; size_t c,op,f,nalloc;
  memset (data,'a',sizeof (data));
  for (c=0;c<sizeof (codecs)/sizeof (codecs[0]);++c) {
    mulib_buffer_t packed={0};
    assert (!codecs[c].encode (data,sizeof (data),&packed));
    for (op=0;op<2;++op) {
      nalloc=SIZE_MAX;
      for (f=0;f<=nalloc;++f) {
        mulib_buffer_t out={0}; unsigned char *before; size_t capacity; int rc;
        assert (mulib_buffer_append (&out,"prefix",6)); before=out.data; capacity=out.capacity; calls=0; fail_at=f;
        rc=op?codecs[c].decode (packed.data,packed.length,sizeof (data),&out):codecs[c].encode (data,sizeof (data),&out);
        if (!f) { assert (!rc); nalloc=calls; }
        else {
          assert (rc==MULIB_CODEC_ENOMEM);
          assert (out.length==6 && !memcmp (out.data,"prefix",6));
          assert (out.data==before && out.capacity==capacity);
        }
        fail_at=0; mulib_buffer_free (&out); assert (live==1);
      }
    }
    mulib_buffer_free (&packed); assert (!live);
  }
  return 0;
}
