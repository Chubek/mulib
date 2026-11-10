#define MULIB_IMPL_COMPILER_DRIVER
#include "mulib-compiler-driver.h"
#include <assert.h>
static int increment(void *program,void *context) {(void)context;++*(int *)program;return 1;}
int main(void) {
    mulib_compiler_driver_t driver={0};int program=0;size_t failed;
    assert(mulib_compiler_driver_add(&driver,"lower",increment,NULL));
    assert(mulib_compiler_driver_run(&driver,&program,&failed)&&program==1&&failed==1);
    mulib_compiler_driver_free(&driver);return 0;
}
