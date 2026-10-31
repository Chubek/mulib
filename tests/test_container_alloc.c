/* Allocation failure must preserve owned configurations, images and cache entries. */
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
static long fail_after = -1;
static size_t live_allocations;
static int allocation_fails (void)
{
  if (fail_after < 0) return 0;
  if (!fail_after) return 1;
  --fail_after; return 0;
}
static void *checked_malloc (size_t n)
{
  void *p;
  if (allocation_fails ()) return NULL;
  p = malloc (n); if (p) ++live_allocations;
  return p;
}
static void *checked_realloc (void *p, size_t n)
{
  void *next;
  if (!p) return checked_malloc (n);
  if (allocation_fails ()) return NULL;
  next = realloc (p, n);
  return next;
}
static void *checked_calloc (size_t count, size_t width)
{
  void *p;
  if (width && count > SIZE_MAX / width) return NULL;
  p = checked_malloc (count * width);
  if (p) memset (p, 0, count * width);
  return p;
}
static void checked_free (void *p)
{
  if (p) { assert (live_allocations); --live_allocations; free (p); }
}
#define MULIB_CT_MALLOC checked_malloc
#define MULIB_CT_REALLOC checked_realloc
#define MULIB_CT_FREE checked_free
/* Include the existing VFS/archive/buffer allocation paths in the same sweep. */
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#include "../examples/container-stack-common.h"
int main (void)
{
  mulib_ct_blob image = { 0 };
  mulib_imgbuildspec recipe;
  long failure;
  int successful_build = 0, successful_put = 0, successful_import = 0;
  assert (!example_container_image (&image));
  assert (!mulib_imgbuildspec_init (&recipe, "failure-test"));
  assert (!mulib_imgbuildspec_file (&recipe, "/file", "data", 4));
  for (failure = 0; failure < 80; ++failure)
    {
      mulib_ct_blob output = { 0 };
      size_t before;
      int rc;
      assert (!mulib_ct_blob_assign (&output, "unchanged", 9, 9));
      before = live_allocations; fail_after = failure;
      rc = mulib_imgbuilder_build (&recipe, NULL, &output, NULL);
      fail_after = -1;
      if (rc)
        {
          assert (rc == MULIB_CT_ENOMEM);
          assert (output.length == 9 && !memcmp (output.data, "unchanged", 9));
          assert (live_allocations == before);
        }
      else successful_build = 1;
      mulib_ct_blob_free (&output);
      if (!rc) break;
    }
  assert (successful_build);
  for (failure = 0; failure < 10; ++failure)
    {
      mulib_containerd_cache cache;
      mulib_containerd_cache_lease lease;
      const mulib_ct_blob *borrowed;
      int rc;
      assert (!mulib_containerd_cache_init (&cache, 1, image.length));
      assert (!mulib_containerd_cache_put (&cache, "old", image.data, image.length, 0, 0));
      fail_after = failure;
      rc = mulib_containerd_cache_put (&cache, "new", image.data, image.length, 0, 0);
      fail_after = -1;
      assert (!mulib_containerd_cache_acquire (&cache, rc ? "old" : "new", 0, &lease, &borrowed));
      assert (borrowed->length == image.length && !memcmp (borrowed->data, image.data, image.length));
      assert (!mulib_containerd_cache_release (&cache, lease));
      assert (!mulib_containerd_cache_free (&cache));
      if (!rc) { successful_put = 1; break; }
    }
  assert (successful_put);
  for (failure = 0; failure < 100; ++failure)
    {
      mulib_image2container output = { 0 }, before_output = { 0 };
      size_t before = live_allocations;
      int rc;
      fail_after = failure;
      rc = mulib_image2container_open (image.data, image.length, NULL, NULL, &output, NULL);
      fail_after = -1;
      if (rc)
        {
          assert (rc == MULIB_CT_ENOMEM);
          assert (!memcmp (&output, &before_output, sizeof (output)));
          assert (live_allocations == before);
        }
      else
        {
          mulib_image2container_free (&output); successful_import = 1;
          assert (live_allocations == before); break;
        }
    }
  assert (successful_import);
  {
    unsigned char frame[] = { 0x28, 0xb5, 0x2f, 0xfd, 0x20, 16, 3, 0, 0, 'x' };
    mulib_ct_blob output = { 0 };
    size_t before = live_allocations;
    fail_after = 0;
    /* Invalid content size and unsupported blocks are detected without allocating. */
    assert (mulib_zstd_raw_decode (frame, sizeof (frame), &output, 16) == MULIB_CT_EFORMAT);
    frame[6] = 5;
    assert (mulib_zstd_raw_decode (frame, sizeof (frame), &output, 16) == MULIB_CT_ENOTSUP);
    frame[6] = 131; /* One RLE block producing exactly 16 bytes. */
    assert (mulib_zstd_raw_decode (frame, sizeof (frame), &output, 16) == MULIB_CT_ENOMEM);
    assert (!output.data && !output.length && live_allocations == before);
    fail_after = -1;
  }
  {
    mulib_containerd daemon;
    mulib_container_qvmboost qvm;
    int successful_create = 0;
    assert (!example_container_daemon (&daemon, &qvm));
    for (failure = 0; failure < 100; ++failure)
      {
        size_t before = live_allocations;
        int rc;
        fail_after = failure;
        rc = mulib_containerd_create (&daemon, image.data, image.length, "allocation", "qvm", 0, 0);
        fail_after = -1;
        if (rc)
          {
            assert (rc == MULIB_CT_ENOMEM);
            assert (!daemon.pool.count && !daemon.mounts.count);
            assert (live_allocations == before);
          }
        else { successful_create = 1; break; }
      }
    assert (successful_create);
    assert (!mulib_containerd_free (&daemon, 1));
  }
  {
    mulib_rootfs_t root;
    mulib_rootfs_buildspec empty;
    mulib_ct_blob archive = { 0 };
    mulib_imgbuilder_report report;
    int detail;
    assert (!mulib_rootfs_init (&root, 8, 128));
    assert (!mulib_rootfs_put (&root, "/base", "keep", 4, 0));
    assert (!mulib_rootfs_buildspec_init (&empty, 1, 0, 128));
    fail_after = 0;
    assert (mulib_rootfs_buildspec_apply (&empty, &root, NULL, &detail) == MULIB_CT_ENOMEM);
    assert (detail == MULIB_VFS_ENOMEM);
    fail_after = 1; /* Sorting succeeds, the archive writer allocation fails. */
    assert (mulib_imgbuilder_archive (&root, 65536, &archive) == MULIB_CT_ENOMEM);
    fail_after = 0;
    assert (mulib_imgbuilder_build (&recipe, &root, &archive, &report) == MULIB_CT_ENOMEM);
    assert (report.detail == MULIB_VFS_ENOMEM);
    fail_after = -1;
    assert (root.storage.count == 1 && root.storage.nodes[0].length == 4);
    mulib_rootfs_buildspec_free (&empty); mulib_rootfs_free (&root);
    mulib_ct_blob_free (&archive);
  }
  mulib_imgbuildspec_free (&recipe); mulib_ct_blob_free (&image);
  assert (live_allocations == 0);
  return 0;
}
