/* Standalone closure only. Omit this file when the host already provides
 * these kernels from its own single implementation translation unit. */
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_HTTPS
#define MULIB_IMPL_TSL
#define MULIB_IMPL_SOCKET
#define MULIB_IMPL_MUW3
#include "detail/mulib-tls-implementation.h"
#include "mulib-muw3.h"
