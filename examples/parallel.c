/* Build with MuLIB's existing Mune and dependency implementations.
 * The implementation switches below follow manual/mulib-mune.md. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_PARALLEL_SH
#include "mulib-parallel-sh.h"
int main(int argc,char **argv){return mulib_parallel_sh_main(argc,argv);}
