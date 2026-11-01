#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#include "virtio.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
  unsigned supported = 0;
  for (size_t i = 0; i < mulib_virtio_profile_count(); ++i) {
    const mulib_virtio_profile_t *p = mulib_virtio_profile_at(i);
    printf("%2u %-20s %s\n", p->id, p->name,
           p->support == MULIB_VIRTIO_PROFILE ? "implemented profile" : "unsupported");
    supported += p->support == MULIB_VIRTIO_PROFILE;
  }
  assert(supported == 21 && mulib_virtio_profile_count() == 48);
  assert(!mulib_virtio_profile(14));
  return 0;
}
