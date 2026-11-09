#define MULIB_IMPL_XTAS
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-xtas.h"
#include <assert.h>
static int count(void *user, const mulib_xtas_event_t *event) {
    unsigned *n = (unsigned *)user; assert(event->node); ++*n; return 1;
}
int main(void) {
    mulib_dsdom_t *node = mulib_dsdom_element("paragraph","text");
    unsigned n = 0;
    assert(node && mulib_xtas_stream(node,count,&n) == 1 && n == 3);
    mulib_xml_destroy(node); return 0;
}
