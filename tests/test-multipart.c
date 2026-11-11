#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_MULTIPART
#include "mulib-multipart.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  char b[256];
  assert (mulib_multipart_field ("abc", "title", "xyz", b, sizeof b) > 0);
  assert (strstr (b, "name=\"title\""));
  return 0;
}
