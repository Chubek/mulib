#include "../notation-common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
    const char source[]="forall(x, x in real -> x^2 >= 0)";
    mulib_eqn_t *equation=mulib_eqn_parse(NULL,source,sizeof source-1,NULL);
    mulib_xml_node_t *math;
    char *latex,*xml;assert(equation);
    latex=mulib_eqn_latex(equation,NULL,NULL);math=mulib_eqn_mathml(equation,1,NULL);
    xml=mulib_mathml_serialize(math,NULL,NULL);assert(latex&&xml);
    puts(latex);puts(xml);free(latex);free(xml);mulib_xml_destroy(math);mulib_eqn_destroy(equation);return 0;
}
