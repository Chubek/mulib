#define MULIB_IMPL_DEFUSE
#include "mulib-defuse.h"
#include <assert.h>
int main(void) {
    mulib_defuse_inst_t code[]={{0,{0,0},0,0},{1,{0,0},1,0}};
    mulib_defuse_info_t info[2];
    assert(mulib_defuse_analyze(code,2,2,info)&&info[0].first_definition==0&&info[0].use_count==1);
    return 0;
}
