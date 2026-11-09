#define MULIB_IMPL_GVIZZ
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-gvizz.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    const char source[] = "Parser: Read source\nParser -> Renderer: DocstrIL\n";
    mulib_gvizz_t *graph = mulib_gvizz_parse(source,sizeof source-1,NULL);
    mulib_dsdom_t *svg;
    char *text;
    assert(graph && mulib_gvizz_node_count(graph) == 2);
    svg = mulib_gvizz_svg(graph); assert(svg);
    text = mulib_xml_serialize(svg,NULL); assert(text); puts(text);
    free(text); mulib_xml_destroy(svg); mulib_gvizz_destroy(graph); return 0;
}
