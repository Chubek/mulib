#define MULIB_IMPL_SQL
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#include "mulib-sql.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_memtable_t t = { 0 };
  mulib_buffer_t value = { 0 };
  const char *s = "INSERT INTO kv VALUES ('k','v')";
  assert (!mulib_sql_execute (&t, s, strlen (s), NULL, NULL));
  s = "SELECT value FROM kv WHERE key='k'";
  assert (!mulib_sql_execute (&t, s, strlen (s), &value, NULL)
          && value.length == 1);
  mulib_buffer_free (&value);
  mulib_memtable_free (&t);
  return 0;
}
