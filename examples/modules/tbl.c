#define MULIB_IMPL_TBL
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSV
#define MULIB_IMPL_TEX
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-tbl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
    const char source[]="| Feature | Value |\n| :--- | ---: |\n| Columns | 2 |\n";
    mulib_tbl_t *table=mulib_tbl_parse(source,sizeof source-1,"pipe",NULL);
    char *html;assert(table&&mulib_tbl_columns(table)==2);
    html=mulib_tbl_render(table,"html",NULL,NULL);assert(html);puts(html);
    free(html);mulib_xml_destroy(table);return 0;
}
