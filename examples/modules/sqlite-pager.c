#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_SQLITE_FORMAT
#define MULIB_IMPL_SQLITE_PAGER
#include "mulib-sqlite-pager.h"
#include <assert.h>

int main(void) {
  mulib_sqlite_pager_t *pager=NULL;
  mulib_sqlite_format_builder_t builder;
  size_t length;
  assert(!mulib_sqlite_pager_open(":memory:",0,0,&pager));
  assert(!mulib_sqlite_format_builder_init(&builder,4096,0));
  assert(!mulib_sqlite_format_table(&builder,1,NULL,0));
  mulib_sqlite_format_finish(&builder);
  assert(!mulib_sqlite_pager_commit(pager,builder.image.data,builder.image.length));
  assert(mulib_sqlite_pager_data(pager,&length) && length==4096);
  mulib_sqlite_format_builder_free(&builder); mulib_sqlite_pager_close(pager); return 0;
}
