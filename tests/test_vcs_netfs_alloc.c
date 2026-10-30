#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static size_t allocation_calls, fail_at = SIZE_MAX, live;
static void *
test_malloc (size_t n)
{
  void *p;
  if (allocation_calls++ == fail_at)
    return NULL;
  p = malloc (n);
  if (p)
    ++live;
  return p;
}
static void *
test_calloc (size_t n, size_t stride)
{
  void *p;
  if (allocation_calls++ == fail_at)
    return NULL;
  p = calloc (n, stride);
  if (p)
    ++live;
  return p;
}
static void *
test_realloc (void *old, size_t n)
{
  int fresh = old == NULL;
  void *p;
  assert (n);
  if (allocation_calls++ == fail_at)
    return NULL;
  p = realloc (old, n);
  if (p && fresh)
    ++live;
  return p;
}
static void
test_free (void *p)
{
  if (p)
    {
      assert (live);
      --live;
      free (p);
    }
}
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_GIT
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_VCSREPO
#define MULIB_IMPL_VCSCOMMIT
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ADLER32
#define MULIB_IMPL_VCSPACK
#define MULIB_IMPL_VFS
#define MULIB_IMPL_FUSE
#define MULIB_IMPL_XTKS
#define MULIB_IMPL_NETFS
#include "mulib-netfs.h"
#include "mulib-vcscommit.h"
#include "mulib-vcspack.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
static void
fail_on (size_t index)
{
  allocation_calls = 0;
  fail_at = index;
}
static void
repository (void)
{
  mulib_vcsrepo_t r = { 0 };
  mulib_git_oid_t id, sentinel = { { 42 } };
  mulib_vcscommit_t c = { 0 };
  for (size_t i = 0; i < 2; ++i)
    {
      fail_on (i);
      assert (!mulib_vcsrepo_init (&r, 8, 2, 8192));
      assert (!r.objects && !r.refs && !live);
    }
  fail_on (SIZE_MAX);
  assert (mulib_vcsrepo_init (&r, 8, 2, 8192));
  {
    size_t baseline = live;
    fail_on (0);
    id = sentinel;
    assert (!mulib_vcsrepo_put (&r, "blob", "hello", 5, &id));
    assert (!r.count && !r.bytes && live == baseline
            && !memcmp (&id, &sentinel, sizeof id));
  }
  fail_on (SIZE_MAX);
  assert (mulib_vcsrepo_put (&r, "tree", NULL, 0, &c.tree) == 1);
  c.author.name = "Author";
  c.author.email = "author@example.test";
  c.committer = c.author;
  c.message = "message";
  c.message_length = 7;
  {
    size_t baseline = live;
    int completed = 0;
    for (size_t i = 0; i < 32 && !completed; ++i)
      {
        char *p = NULL;
        size_t n = 123;
        fail_on (i);
        completed = mulib_vcscommit_encode (&c, &p, &n);
        if (completed)
          {
            assert (p && n > 7);
            test_free (p);
          }
        else
          assert (!p && n == 123);
        assert (live == baseline);
      }
    assert (completed);
  }
  {
    size_t baseline = live;
    int completed = 0;
    for (size_t i = 0; i < 32 && !completed; ++i)
      {
        id = sentinel;
        fail_on (i);
        completed = mulib_vcscommit_store (&r, &c, &id);
        if (!completed)
          {
            assert (r.count == 1 && live == baseline);
            assert (!memcmp (&id, &sentinel, sizeof id));
          }
      }
    assert (completed && r.count == 2);
  }
  fail_on (SIZE_MAX);
  mulib_vcsrepo_free (&r);
  assert (!live);
  {
    mulib_vcspack_object_t object = { 3, "test", 4 };
    unsigned char *p = NULL;
    size_t n = 123;
    fail_on (0);
    assert (!mulib_vcspack_encode (&object, 1, &p, &n) && !p && n == 123
            && !live);
  }
}
static int
answer (void *ctx, const void *req, size_t n, void *reply, size_t cap,
        size_t *used)
{
  unsigned char *p = reply;
  (void)n;
  ++*(int *)ctx;
  assert (cap >= 36);
  memset (p, 0, 36);
  memcpy (p, "MNR1", 4);
  memcpy (p + 8, (const unsigned char *)req + 8, 8);
  mulib_endian_store_be32 (p + 16, MULIB_VFS_DIRECTORY);
  *used = 36;
  return 1;
}
static void
netfs (void)
{
  mulib_netfs_t client;
  mulib_vfs_ops ops = mulib_netfs_ops ();
  mulib_vfs vfs;
  mulib_vfs_stat st;
  size_t baseline;
  int calls = 0;
  fail_on (SIZE_MAX);
  assert (mulib_netfs_init (&client, answer, &calls, 1024));
  assert (mulib_vfs_init (&vfs, 1) == MULIB_VFS_OK);
  assert (mulib_vfs_mount (&vfs, "/", &ops, &client) == MULIB_VFS_OK);
  baseline = live;
  for (size_t i = 0; i < 2; ++i)
    {
      st.kind = MULIB_VFS_FILE;
      st.size = 123;
      fail_on (i);
      assert (mulib_vfs_getstat (&vfs, "/", &st) == MULIB_VFS_ENOMEM);
      assert (st.kind == MULIB_VFS_FILE && st.size == 123 && live == baseline
              && !client.busy && !calls);
    }
  fail_on (SIZE_MAX);
  assert (mulib_vfs_getstat (&vfs, "/", &st) == MULIB_VFS_OK);
  assert (calls == 1 && st.kind == MULIB_VFS_DIRECTORY && live == baseline);
  mulib_vfs_free (&vfs);
  assert (!live);
}
int
main (void)
{
  repository ();
  netfs ();
  return 0;
}
