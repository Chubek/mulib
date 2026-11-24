/* Native fixture for the closed Linux AMD64 companion recipe. A single C TU
 * implements the canonical MANIFEST sandbox-profile -> ipc-framed closure. */
#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_IPC_FRAMED
#include <mulib-sandbox-profile.h>
#include <mulib-ipc-framed.h>
