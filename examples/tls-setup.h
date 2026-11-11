/* Complete native TLS dependency setup for standalone examples. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "detail/mulib-tls-implementation.h"
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_URI
#define MULIB_IMPL_SOCKET
#define MULIB_IMPL_TSL
#include "mulib-tls.h"
