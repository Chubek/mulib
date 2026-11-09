#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-dsdom.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    mulib_dsdom_t *node = mulib_dsdom_element("paragraph","literal <text>"), *copy;
    char *text;
    assert(node && mulib_xml_set_attr(node,"id","intro"));
    copy = mulib_dsdom_clone(node); assert(copy && mulib_dsdom_find_id(copy,"intro") == copy);
    text = mulib_dsdom_text(copy,NULL); assert(text && !strcmp(text,"literal <text>"));
    free(text); mulib_xml_destroy(copy); mulib_xml_destroy(node); return 0;
}
