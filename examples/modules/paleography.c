#include "../notation-common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
    const char script[]="lpaleo.define{name='triangle',codepoint=57344,advance=700,path='M 0 0 L 600 0 L 300 700 Z'};lpaleo.script('imaginary',{mapping={a='triangle'}})";
    mulib_paleography_t *p=mulib_paleography_create(NULL,NULL);mulib_xml_node_t *svg;char *text;
    assert(p&&mulib_paleography_extend(p,script,strlen(script),NULL));
    svg=mulib_paleography_svg(p,"aaa",3,"imaginary",NULL,NULL);assert(svg);
    text=mulib_xml_serialize(svg,NULL);assert(text);puts(text);free(text);mulib_xml_destroy(svg);
    text=mulib_paleography_package(p,NULL,NULL);assert(text);puts(text);free(text);mulib_paleography_destroy(p);return 0;
}
