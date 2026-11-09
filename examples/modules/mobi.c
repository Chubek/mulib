#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_MOBI
#include "mulib-mobi.h"
#include <assert.h>
int
main (void)
{
  /* A minimal uncompressed PalmDOC database with a header and one text record.
   */
  unsigned char pdb[117] = { 0 };
  mulib_mobi_reader_t reader;
  mulib_buffer_t text = { 0 };
  memcpy (pdb, "Hello", 5);
  memcpy (pdb + 60, "TEXtREAd", 8);
  pdb[77] = 2;
  pdb[81] = 96;
  pdb[89] = 112;
  pdb[97] = 1;
  pdb[103] = 5;
  pdb[105] = 1;
  pdb[106] = 0x10;
  memcpy (pdb + 112, "Hello", 5);
  assert (mulib_mobi_open (&reader, pdb, sizeof (pdb)) == 0);
  assert (mulib_mobi_text (&reader, 100, &text) == 0);
  assert (text.length == 5 && !memcmp (text.data, "Hello", 5));
  mulib_buffer_free (&text);
  return 0;
}
