#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_MOBI
#define MULIB_IMPL_AZW3
#include "mulib-azw3.h"
#include <assert.h>
static void
be32 (unsigned char *p, unsigned v)
{
  p[0] = (unsigned char)(v >> 24);
  p[1] = (unsigned char)(v >> 16);
  p[2] = (unsigned char)(v >> 8);
  p[3] = (unsigned char)v;
}
int
main (void)
{
  /* Minimal synthetic KF8 header and a single raw text flow. */
  unsigned char pdb[365] = { 0 }, *h = pdb + 96;
  mulib_azw3_reader_t reader;
  mulib_buffer_t text = { 0 };
  memcpy (pdb + 60, "BOOKMOBI", 8);
  pdb[77] = 2;
  be32 (pdb + 78, 96);
  be32 (pdb + 86, 360);
  h[1] = 1;
  be32 (h + 4, 5);
  h[9] = 1;
  h[10] = 16;
  memcpy (h + 16, "MOBI", 4);
  be32 (h + 20, 248);
  be32 (h + 24, 2);
  be32 (h + 28, 65001);
  be32 (h + 36, 8);
  be32 (h + 104, 8);
  be32 (h + 108, UINT32_MAX);
  be32 (h + 192, UINT32_MAX);
  be32 (h + 196, 1);
  memcpy (pdb + 360, "Hello", 5);
  assert (mulib_azw3_open (&reader, pdb, sizeof (pdb)) == 0);
  assert (mulib_azw3_flow (&reader, 0, 100, &text) == 0);
  assert (text.length == 5 && !memcmp (text.data, "Hello", 5));
  mulib_buffer_free (&text);
  return 0;
}
