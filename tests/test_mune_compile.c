/* Compile only against the real public Mune API; no runtime implementation. */
#define MULIB_IMPL_PARALLEL_SH
#define MULIB_IMPL_SHEBANG_SH
#include "../kernels/mulib-parallel-sh.h"
#include "../kernels/mulib-shebang-sh.h"
int main(void){return 0;}
