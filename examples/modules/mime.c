#define MULIB_IMPL_MIME
#define MULIB_IMPL_SECURITY
#include "mulib-mime.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  assert (!strcmp (mulib_mime_type ("picture.PNG"), "image/png"));
  assert (!strcmp (mulib_mime_type ("unknown"), "application/octet-stream"));
  return 0;
}
