#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t alloc_attempts, fail_at = SIZE_MAX;

static void *
storage_malloc (size_t n)
{
  return alloc_attempts++ == fail_at ? NULL : malloc (n);
}

static void *
storage_realloc (void *p, size_t n)
{
  return alloc_attempts++ == fail_at ? NULL : realloc (p, n);
}

#define malloc storage_malloc
#define realloc storage_realloc
#define MULIB_IMPL_WEBSTORAGE
#include "mulib-webstorage.h"
#undef realloc
#undef malloc

#include "mulib-indexeddb-cursor.h"
#include "mulib-indexeddb-index.h"
#include "mulib-web-cachestorage.h"
#include "mulib-web-opfs.h"
#include "mulib-web-storagequota.h"

static mulib_indexeddb_key_t
number (double n)
{
  mulib_indexeddb_key_t k = { mulib_indexeddb_key_NUMBER, n, { NULL, 0 } };
  return k;
}

static void
test_indexeddb (void)
{
  const unsigned char bytes[] = { 'a', 0, 'b' };
  mulib_indexeddb_key_t text = {
    mulib_indexeddb_key_STRING, 0, { bytes, sizeof bytes }
  };
  mulib_indexeddb_key_t empty = {
    mulib_indexeddb_key_STRING, 0, { NULL, 0 }
  };
  mulib_indexeddb_store_record records[4] = { 0 }, scratch[4];
  mulib_indexeddb_store_t db = { records, 0, 4 };
  uintptr_t out = 77;
  assert (!mulib_indexeddb_store_get (&db, number (0), &out) && out == 77);
  assert (mulib_indexeddb_store_apply (&db, NULL, 0, NULL, 0));
  assert (!mulib_indexeddb_store_put (&db, number (NAN), 2, 0));
  assert (mulib_indexeddb_store_put (&db, text, 4, 0));
  assert (mulib_indexeddb_store_put (&db, number (-0.0), 0, 0));
  assert (mulib_indexeddb_store_get (&db, number (+0.0), &out) && out == 0);
  assert (mulib_indexeddb_store_put (&db, empty, 3, 0));
  assert (mulib_indexeddb_store_put (&db, number (-INFINITY), 1, 0));
  assert (!mulib_indexeddb_store_put (&db, number (INFINITY), 7, 0));
  assert (!mulib_indexeddb_index_unique (&records[1].key, 1, number (+0.0)));
  assert (records[0].value == 1 && records[1].value == 0 &&
          records[2].value == 3 && records[3].value == 4);

  const mulib_indexeddb_store_op ops[] = {
    { mulib_indexeddb_store_DELETE, number (-INFINITY), 0 },
    { mulib_indexeddb_store_ADD, number (INFINITY), 2 },
    { mulib_indexeddb_store_PUT, number (+0.0), 9 },
    { mulib_indexeddb_store_ADD, text, 8 },
  };
  unsigned char before[sizeof records];
  memcpy (before, records, sizeof before);
  assert (!mulib_indexeddb_store_apply (&db, ops, 4, scratch, 4));
  const mulib_indexeddb_store_op overflow[] = {
    { mulib_indexeddb_store_PUT, number (+0.0), 16 },
    { mulib_indexeddb_store_ADD, number (INFINITY), 17 },
  };
  assert (!mulib_indexeddb_store_apply (&db, overflow, 2, scratch, 4));
  assert (!mulib_indexeddb_store_apply (&db, ops, 3, scratch, 3));
  assert (!mulib_indexeddb_store_apply (&db, ops, 3, records, 4));
  assert (db.len == 4 && !memcmp (before, records, sizeof before));
  assert (mulib_indexeddb_store_apply (&db, ops, 3, scratch, 4));
  assert (db.len == 4 && records[0].value == 9 && records[1].value == 2);
  assert (mulib_indexeddb_store_delete (&db, empty));
  assert (mulib_indexeddb_store_delete (&db, empty) && db.len == 3);
  assert (!mulib_indexeddb_store_get (&db, empty, &out) && out == 0);
  assert (mulib_indexeddb_store_get (&db, text, &out) && out == 4);
  mulib_indexeddb_cursor_t reverse = { records, db.len, 0, 1 };
  assert (mulib_indexeddb_cursor_next (&reverse)->value == 4);
  assert (mulib_indexeddb_cursor_next (&reverse)->value == 2);
  assert (mulib_indexeddb_cursor_next (&reverse)->value == 9);
  assert (!mulib_indexeddb_cursor_next (&reverse));

  mulib_indexeddb_store_t no_capacity = { NULL, 0, 0 };
  assert (!mulib_indexeddb_store_put (&no_capacity, empty, 0, 0));
  assert (mulib_indexeddb_store_delete (&no_capacity, empty));
  mulib_indexeddb_store_t bad = { NULL, 0, 1 };
  assert (!mulib_indexeddb_store_put (&bad, empty, 0, 0));
  bad.v = records;
  bad.cap = SIZE_MAX;
  assert (!mulib_indexeddb_store_delete (&bad, empty));
}

static void
test_webstorage (void)
{
  mulib_webstorage_t s = { 0 };
  for (size_t fault = 0; fault < 3; fault++)
    {
      fail_at = alloc_attempts + fault;
      assert (!mulib_webstorage_set_limited (&s, "b", "1", 2, 8));
      assert (!s.entries && !s.len && !s.cap);
    }
  fail_at = SIZE_MAX;
  size_t attempts = alloc_attempts;
  assert (!mulib_webstorage_set_limited (&s, "", "", 0, 2));
  assert (!mulib_webstorage_set_limited (&s, "", "", 1, 1));
  assert (alloc_attempts == attempts);
  char b[] = "b";
  assert (mulib_webstorage_set_limited (&s, b, "1", 2, 8));
  b[0] = 'z';
  assert (mulib_webstorage_set_limited (&s, "a", "2", 2, 8));
  const char *b_key = mulib_webstorage_key (&s, 0);
  const char *old = mulib_webstorage_get (&s, b_key);
  assert (!strcmp (b_key, "b") && !strcmp (old, "1"));
  attempts = alloc_attempts;
  assert (!mulib_webstorage_set_limited (&s, "c", "", 2, SIZE_MAX));
  assert (!mulib_webstorage_set_limited (&s, b_key, "xx", 2, 8));
  assert (alloc_attempts == attempts);
  fail_at = alloc_attempts;
  assert (!mulib_webstorage_set_limited (&s, b_key, "", 2, 7));
  fail_at = SIZE_MAX;
  assert (mulib_webstorage_get (&s, b_key) == old);
  assert (mulib_webstorage_set_limited (&s, b_key, old, 2, 8));
  assert (mulib_webstorage_set_limited (&s, b_key, "", 2, 7));
  assert (mulib_webstorage_key (&s, 0) == b_key);
  assert (mulib_webstorage_remove (&s, b_key));
  assert (!mulib_webstorage_remove (&s, "b"));
  assert (!strcmp (mulib_webstorage_key (&s, 0), "a"));
  assert (mulib_webstorage_set_limited (&s, "", "", 2, 6));
  assert (!strcmp (mulib_webstorage_key (&s, 1), ""));
  size_t cap = s.cap;
  mulib_webstorage_clear (&s);
  mulib_webstorage_clear (&s);
  assert (!s.len && s.cap == cap && !mulib_webstorage_key (&s, 0));
  assert (mulib_webstorage_set_limited (&s, "x", "y", 1, 4));
  mulib_webstorage_free (&s);
  mulib_webstorage_free (&s);
  assert (!s.entries && !s.cap && !s.len);
}

static void
test_cache (void)
{
  mulib_web_cachestorage_entry entries[2];
  mulib_web_cachestorage_t cache = { entries, 0, 2 };
  muba_span get = muba_str ("GET"), head = muba_str ("HEAD");
  muba_span a = muba_str ("https://offline.test/a?q=1");
  muba_span b = muba_str ("https://offline.test/a?q=2");
  muba_span c = muba_str ("https://offline.test/b");
  assert (!mulib_web_cachestorage_put (&cache, head, a, 1));
  assert (!mulib_web_cachestorage_put (&cache, get, a, 0));
  assert (!mulib_web_cachestorage_put (&cache, get, muba_str (""), 1));
  assert (mulib_web_cachestorage_put (&cache, get, a, 1));
  assert (mulib_web_cachestorage_put (&cache, get, b, 2));
  assert (!mulib_web_cachestorage_put (&cache, get, c, 3));
  assert (mulib_web_cachestorage_put (&cache, get, a, 4));
  assert (mulib_web_cachestorage_match (cache.v, cache.len, get, a) == 4);
  assert (!mulib_web_cachestorage_match (cache.v, cache.len, head, a));
  assert (!mulib_web_cachestorage_delete (&cache, head, a));
  assert (mulib_web_cachestorage_delete (&cache, get, a));
  assert (!mulib_web_cachestorage_delete (&cache, get, a));
  assert (!mulib_web_cachestorage_match (cache.v, cache.len, get, a));
  assert (mulib_web_cachestorage_match (cache.v, cache.len, get, b) == 2);
  assert (mulib_web_cachestorage_put (&cache, get, c, 3));
}

static void
test_quota_and_path (void)
{
  mulib_web_storagequota_t q = { SIZE_MAX - 1, SIZE_MAX };
  assert (mulib_web_storagequota_reserve (&q, 1) && q.used == SIZE_MAX);
  assert (!mulib_web_storagequota_reserve (&q, 1) && q.used == SIZE_MAX);
  assert (mulib_web_storagequota_replace (&q, 15, 15) && q.used == SIZE_MAX);
  assert (!mulib_web_storagequota_replace (&q, 1, 2) && q.used == SIZE_MAX);
  assert (mulib_web_storagequota_release (&q, SIZE_MAX - 10) && q.used == 10);
  assert (!mulib_web_storagequota_release (&q, 11) && q.used == 10);
  q.limit = 5;
  assert (!mulib_web_storagequota_replace (&q, 1, 2) && q.used == 10);
  assert (mulib_web_storagequota_replace (&q, 3, 1) && q.used == 8);
  assert (mulib_web_storagequota_release (&q, 3) && q.used == 5);
  assert (mulib_web_storagequota_replace (&q, 2, 1) && q.used == 4);
  assert (mulib_web_storagequota_reserve (&q, 1) && q.used == 5);

  const char *bad[] = { "", "/", "/a", "a/", "a//b", ".", "..",
                        "a/../b", "a/.", "a\\b", "C:/a", "a:b" };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
    assert (!mulib_web_opfs_relative_path (muba_str (bad[i]), 16, 3));
  const unsigned char nul[] = { 'a', '/', 0, 'b' };
  muba_span embedded = { nul, sizeof nul };
  assert (!mulib_web_opfs_relative_path (embedded, sizeof nul, 2));
  assert (!mulib_web_opfs_relative_path (muba_str ("a/b"), 2, 2));
  assert (!mulib_web_opfs_relative_path (muba_str ("a/b"), 3, 1));
  assert (!mulib_web_opfs_relative_path (muba_str ("a"), 1, 0));
  assert (mulib_web_opfs_relative_path (muba_str ("a/b"), 3, 2));
  assert (mulib_web_opfs_relative_path (muba_str ("a"), 1, 1));
}

int
main (void)
{
  test_indexeddb ();
  test_webstorage ();
  test_cache ();
  test_quota_and_path ();
  puts ("offline browser storage: 4 groups passed");
  return 0;
}
