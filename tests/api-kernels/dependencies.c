/* Link against the real repository kernels, never tests/api-kernels/stubs. */
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_SSE
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_URL
#define MULIB_IMPL_XML
#include "../fixtures/muw3-setup.h"
#include "mulib-json.h"
#include "mulib-sse.h"
#include "mulib-base64.h"
#include "mulib-url.h"
#include "mulib-xml.h"
