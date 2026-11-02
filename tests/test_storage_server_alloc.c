/* Exercise allocation failures in the same dependency-complete TU. */
#include <stdlib.h>
static long allocation_budget = -1;
static int
fail_allocation (void)
{
  if (allocation_budget < 0)
    return 0;
  if (!allocation_budget)
    return 1;
  --allocation_budget;
  return 0;
}
static void *
test_malloc (size_t n)
{
  return fail_allocation () ? NULL : malloc (n);
}
static void *
test_calloc (size_t n, size_t z)
{
  return fail_allocation () ? NULL : calloc (n, z);
}
static void *
test_realloc (void *p, size_t n)
{
  return fail_allocation () ? NULL : realloc (p, n);
}
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define main storage_server_baseline_main
#include "test_storage_server.c"
#undef main
#undef malloc
#undef calloc
#undef realloc

int
main (void)
{
  mulib_json_t *v = json ("{\"a\":[1,2,3],\"b\":\"text\"}");
  for (long budget = 0; budget < 80; ++budget)
    {
      for (int format = 0; format <= 3; ++format)
        {
          mulib_buffer_t b = { 0 };
          int ok;
          assert (mulib_buffer_append (&b, "prefix", 6));
          allocation_budget = budget;
          ok = mulib_dbserializer_write ((mulib_dbformat)format, v, &b);
          allocation_budget = -1;
          assert (!memcmp (b.data, "prefix", 6));
          if (!ok)
            assert (b.length == 6);
          else
            {
              mulib_json_t *copy;
              allocation_budget = budget;
              copy = mulib_dbdeserializer_read ((mulib_dbformat)format,
                                                b.data + 6, b.length - 6);
              allocation_budget = -1;
              mulib_json_destroy (copy);
            }
          mulib_buffer_free (&b);
        }
      {
        mulib_documentdb_t db = { 0 };
        mulib_json_t *copy;
        int rc;
        assert (!mulib_documentdb_put (&db, "a", 1, v, 1));
        allocation_budget = budget;
        rc = mulib_documentdb_put (&db, "b", 1, v, 2);
        allocation_budget = -1;
        if (rc)
          assert (db.records.count == 1);
        copy = mulib_documentdb_get (&db, "a", 1);
        assert (copy);
        mulib_json_destroy (copy);
        mulib_documentdb_free (&db);
      }
      {
        mulib_column_rdbms_t db = { NULL, 2, 0 };
        const double a[] = { 1, 2 };
        int ok;
        assert (mulib_column_rdbms_append (&db, a, 2));
        allocation_budget = budget;
        ok = mulib_column_rdbms_append (&db, a, 2);
        allocation_budget = -1;
        assert (db.rows == (ok ? 2U : 1U));
        assert (mulib_column_rdbms_column (&db, 1)[0] == 2);
        mulib_column_rdbms_free (&db);
      }
      {
        mulib_http_message_t response = { 0 };
        mulib_http_result rc;
        allocation_budget = budget;
        rc = mulib_http_response_create (&response, 200, "text/plain", "abc",
                                         3);
        allocation_budget = -1;
        if (rc != MULIB_HTTP_OK)
          assert (!response.body && !response.header_count);
        mulib_http_message_free (&response);
      }
    }
  mulib_json_destroy (v);
  puts ("storage/server allocation failures: OK");
  return 0;
}
