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
  char type[64];
  assert (
      mulib_mime_content_type ("Text/HTML; charset=UTF-8", type, sizeof type));
  assert (!strcmp (type, "text/html"));
  return 0;
}
