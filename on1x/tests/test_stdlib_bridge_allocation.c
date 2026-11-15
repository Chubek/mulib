/* Only adapter allocation fails; the separately compiled canonical INI kernel
 * retains its allocator. Each partial failure must release temporary copies. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int remaining=-1;
static size_t live;
static void *checked_malloc(size_t n) {
  if(remaining==0)return NULL;
  if(remaining>0)--remaining;
  void *p=malloc(n);if(p)++live;return p;
}
static void checked_free(void *p) { if(p) { assert(live);--live;free(p); } }
#define ON1X_BRIDGE_INI
#define ON1X_BRIDGE_MALLOC checked_malloc
#define ON1X_BRIDGE_POOL_BYTES 128
#define free checked_free
#include "../stdlib/_bridge/mulib_bridge.c"
#undef free
static void check_cleanup(void) { assert(live==0); }
int main(void) {
  assert(!atexit(check_cleanup));
  for(int i=0;i<2;++i) {
    remaining=i;
    assert(!strcmp(on1x_mulib_ini_get("x=abc\n","","x"),""));
    assert(on1x_mulib_bridge_status()==ON1X_BRIDGE_MEMORY);
    assert(live==0);
  }
  remaining=-1;
  assert(!strcmp(on1x_mulib_ini_get("x=\n","","x"),""));
  assert(on1x_mulib_bridge_status()==ON1X_BRIDGE_OK && live==1);
  assert(!strcmp(on1x_mulib_ini_get("x=value\ninvalid\n","","x"),""));
  assert(on1x_mulib_bridge_status()==ON1X_BRIDGE_PARSE && live==1);
  assert(!on1x_mulib_ini_has(NULL,"","x"));
  assert(on1x_mulib_bridge_status()==ON1X_BRIDGE_INVALID);
  while(on1x_mulib_bridge_status()!=ON1X_BRIDGE_LIMIT)
    (void)on1x_mulib_ini_get("x=abc\n","","x");
  size_t retained=live;
  assert(!strcmp(on1x_mulib_ini_get("x=abc\n","","x"),""));
  assert(on1x_mulib_bridge_status()==ON1X_BRIDGE_LIMIT && live==retained);
  return 0; /* cleanup runs before check_cleanup, including on sanitizer builds */
}
