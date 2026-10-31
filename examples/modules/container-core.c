#include "include-mulib-container-core.h"
#include <assert.h>
int main (void)
{
  mulib_ct_blob bytes = { 0 };
  assert (mulib_ct_id_valid ("worker-1"));
  assert (!mulib_ct_blob_append (&bytes, "hello", 5, 64));
  assert (!mulib_ct_blob_assign (&bytes, bytes.data + 1, 4, 64));
  assert (bytes.length == 4 && !memcmp (bytes.data, "ello", 4));
  mulib_ct_blob_free (&bytes);
  return 0;
}
