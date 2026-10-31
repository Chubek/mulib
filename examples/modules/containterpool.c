#include "../container-stack-common.h"
int main (void)
{
  mulib_containterpool pool;
  mulib_containterpool_handle handle;
  mulib_containterpool_entry *entry;
  mulib_containerboost backends;
  mulib_containerboost_backend backend;
  mulib_container_qvmboost config;
  mulib_ct_blob image = { 0 };
  assert (!example_container_image (&image)); example_container_qvm (&config);
  assert (!mulib_containerboost_init (&backends, 2));
  assert (!mulib_container_qvmboost_backend (&config, "qvm", 1, &backend));
  assert (!mulib_containerboost_register (&backends, &backend));
  assert (!mulib_containterpool_init (&pool, 4));
  assert (!mulib_containterpool_add (&pool, image.data, image.length, "one", NULL,
      &backends, NULL, 0, &handle));
  assert (!mulib_containterpool_acquire (&pool, handle, &entry));
  assert (entry->image.container.state == MULIB_CONTAINER_CREATED);
  assert (!mulib_containterpool_release (&pool, handle));
  assert (!mulib_containterpool_remove (&pool, handle, 0));
  assert (!mulib_containterpool_free (&pool, 0));
  mulib_containerboost_free (&backends); mulib_ct_blob_free (&image);
  return 0;
}
