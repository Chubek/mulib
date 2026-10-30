#define MULIB_IMPL_VARINT
#define MULIB_IMPL_IPFS
#include "mulib-ipfs.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  /* SHA-256 of the empty block, obtained independently. */
  mulib_ipfs_cid_t cid
      = { 0x55,
          { 0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
            0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
            0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55 } },
      decoded;
  char text[60];
  assert (mulib_ipfs_cid_format (&cid, text));
  assert (!strcmp (
      text, "bafkreihdwdcefgh4dqkjv67uzcmw7ojee6xedzdetojuzjevtenxquvyku"));
  assert (mulib_ipfs_cid_parse (text, strlen (text), &decoded));
  assert (!memcmp (decoded.digest, cid.digest, 32));
  return 0;
}
