#include "../notation-common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
    const char json[]="[{\"label\":\"A\",\"value\":30},{\"label\":\"B\",\"value\":70}]";
    mulib_chart_error_t error;mulib_chart_t *c=mulib_chart_create(NULL,&error);
    mulib_json_t *data=mulib_json_parse(json,sizeof json-1,NULL);mulib_xml_node_t *svg;char *text;
    assert(c&&data);svg=mulib_chart_render(c,"pie",data,NULL,&error);assert(svg);
    text=mulib_xml_serialize(svg,NULL);assert(text);puts(text);
    free(text);mulib_xml_destroy(svg);mulib_json_destroy(data);mulib_chart_destroy(c);return 0;
}
