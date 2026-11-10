#define MULIB_IMPL_DCE
#include "mulib-dce.h"
#include <assert.h>
int main(void) {
    mulib_defuse_inst_t code[]={{0,{0,0},0,0},{1,{0,0},0,0},{SIZE_MAX,{0,0},1,1}};
    unsigned char marks[3];
    assert(mulib_dce_mark(code,3,0,marks)&&marks[0]&&!marks[1]&&marks[2]);
    return 0;
}
