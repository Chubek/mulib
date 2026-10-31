#include "../container-stack-common.h"
int main (void)
{
  mulib_ct_blob image = { 0 };
  mulib_containerd_cache cache;
  mulib_containerd_cache_lease lease;
  const mulib_ct_blob *borrowed;
  assert (!example_container_image (&image));
  assert (!mulib_containerd_cache_init (&cache, 4, 65536));
  assert (!mulib_containerd_cache_put (&cache, "base", image.data, image.length, 100, 10));
  assert (!mulib_containerd_cache_acquire (&cache, "base", 101, &lease, &borrowed));
  assert (borrowed->length == image.length);
  assert (!mulib_containerd_cache_release (&cache, lease));
  assert (mulib_containerd_cache_sweep (&cache, 110) == 1);
  assert (!mulib_containerd_cache_free (&cache)); mulib_ct_blob_free (&image);
  return 0;
}
