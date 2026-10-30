/* Deterministic allocation failure sweeps, including copy-on-write
 * publication. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static long fail_at = -1, calls;
static int
fail (void)
{
  return fail_at >= 0 && calls++ == fail_at;
}
static void *
test_malloc (size_t n)
{
  return fail () ? NULL : malloc (n);
}
static void *
test_calloc (size_t n, size_t z)
{
  return fail () ? NULL : calloc (n, z);
}
static void *
test_realloc (void *p, size_t n)
{
  return fail () ? NULL : realloc (p, n);
}
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_WAL
#define MULIB_IMPL_SSTFILE
#define MULIB_IMPL_LSMTREE
#define MULIB_IMPL_BPTREE
#define MULIB_IMPL_COWBPTREE
#define MULIB_IMPL_ROOTFS
#define MULIB_IMPL_VFS
#define MULIB_IMPL_ARCHIVE
#include "mulib-cowb+tree.h"
#include "mulib-lsmtree.h"
#include "mulib-rootfs.h"
#undef malloc
#undef calloc
#undef realloc
int
main (void)
{
  long i;
  int succeeded = 0;
  for (i = 0; i < 100 && !succeeded; ++i)
    {
      mulib_lsmtree_t t = { 0 };
      mulib_buffer_t value = { 0 };
      size_t oldwal;
      int rc;
      fail_at = -1;
      assert (!mulib_lsmtree_put (&t, "key", 3, "old", 3, 0));
      oldwal = t.wal.length;
      calls = 0;
      fail_at = i;
      rc = mulib_lsmtree_put (&t, "key", 3, "new", 3, 0);
      fail_at = -1;
      assert (!mulib_lsmtree_get (&t, "key", 3, &value) && value.length == 3);
      if (rc)
        {
          assert (t.sequence == 1 && t.wal.length == oldwal
                  && !memcmp (value.data, "old", 3));
        }
      else
        {
          succeeded = 1;
          assert (t.sequence == 2 && !memcmp (value.data, "new", 3));
        }
      mulib_buffer_free (&value);
      mulib_lsmtree_free (&t);
    }
  assert (succeeded);
  succeeded = 0;
  for (i = 0; i < 100 && !succeeded; ++i)
    {
      mulib_cowbptree_t a = { 0 }, b = { 0 };
      uint64_t v;
      int rc, j;
      fail_at = -1;
      for (j = 0; j < 40; ++j)
        assert (mulib_cowbptree_set (&a, j, (uint64_t)j));
      assert (mulib_cowbptree_snapshot (&a, &b));
      calls = 0;
      fail_at = i;
      rc = mulib_cowbptree_set (&a, 20, 999);
      fail_at = -1;
      assert (mulib_cowbptree_get (&b, 20, &v) && v == 20);
      assert (mulib_cowbptree_get (&a, 20, &v) && v == (rc ? 999 : 20));
      succeeded = rc;
      mulib_cowbptree_free (&a);
      mulib_cowbptree_free (&b);
    }
  assert (succeeded);
  {
    mulib_archive_writer_t archive;
    fail_at = -1;
    mulib_archive_writer_init (&archive);
    assert (!mulib_archive_add (&archive, "dir/file", "x", 1, 0644, 0));
    assert (!mulib_archive_add_directory (&archive, "dir", 0755, 0));
    assert (!mulib_archive_add (&archive, "other", "y", 1, 0644, 0));
    assert (!mulib_archive_finish (&archive));
    succeeded = 0;
    for (i = 0; i < 100 && !succeeded; ++i)
      {
        mulib_rootfs_t root = { 0 };
        int rc;
        calls = 0;
        fail_at = i;
        rc = mulib_rootfs_import (archive.bytes.data, archive.bytes.length, 8,
                                  8, &root);
        fail_at = -1;
        if (rc)
          assert (!root.storage.nodes && !root.storage.count);
        else
          {
            assert (root.storage.count == 3);
            succeeded = 1;
          }
        mulib_rootfs_free (&root);
      }
    assert (succeeded);
    mulib_archive_writer_free (&archive);
  }
  return 0;
}
