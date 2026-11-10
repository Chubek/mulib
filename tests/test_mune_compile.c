/* Compile only against the real public Mune API; no runtime implementation. */
#define MULIB_IMPL_PARALLEL_SH
#define MULIB_IMPL_SHEBANG_SH
#include "../include/mulib-parallel-sh.h"
#include "../include/mulib-shebang-sh.h"
int main(void){return 0;}
