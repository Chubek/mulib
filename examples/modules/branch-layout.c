#define MULIB_IMPL_BRANCH_LAYOUT
#include "mulib-branch-layout.h"
#include <assert.h>
int main(void) {
    size_t fixed[]={0,200,1},offsets[3],total;
    mulib_branch_t branches[]={{2,2,5,0},{SIZE_MAX,0,0,0},{SIZE_MAX,0,0,0}};
    unsigned char wide[3];
    assert(mulib_branch_layout(3,fixed,branches,offsets,wide,&total));
    assert(wide[0]&&offsets[2]==205&&total==206);
    return 0;
}
