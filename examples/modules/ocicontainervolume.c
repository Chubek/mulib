#define MULIB_IMPL_OCICONTAINERVOLUME
#define MULIB_IMPL_VFS
#include "mulib-ocicontainervolume.h"
#include <assert.h>
int
main (void)
{
  mulib_ocicontainervolume_t volume;
  assert (mulib_ocicontainervolume_init (&volume, "/host/data", "/data", 1)
          && volume.readonly);
  assert (!mulib_ocicontainervolume_init (&volume, "/host", "/", 0));
  return 0;
}
