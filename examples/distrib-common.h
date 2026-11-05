/* Implementation placement for the standalone distributed-computing examples.
 * Production applications put these switches in exactly one translation unit. */
#ifndef MULIB_EXAMPLE_DISTRIB_COMMON_H
#define MULIB_EXAMPLE_DISTRIB_COMMON_H
#define MULIB_IMPL_DISTRIBCOMP
#define MULIB_IMPL_DISTRIBTASK
#define MULIB_IMPL_MUMQ
#define MULIB_IMPL_MUMQ_SERVER
#define MULIB_IMPL_MUMQ_PUBLISHER
#define MULIB_IMPL_MUMQ_SUBSCRIBER
#define MULIB_IMPL_AES
#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_SOCKET
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DAEMON
#define MULIB_IMPL_PROCESS
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-distribcomp.h"
#include "mulib-mumq-server.h"
#include <assert.h>
#endif
