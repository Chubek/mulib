/* Compile the optional adapters against actual upstream declarations. This is
 * intentionally not a runtime or full Mune-compatibility certification. */
#include "mulib-glx.h"
#include "mulib-mune.h"
#define MULIB_GFX_ENABLE_LEGACY_GLX
#define MULIB_GFX_ENABLE_MUNE
#include "mulib-glx-legacy-bridge.h"
#include "mulib-immgtk-mune-bridge.h"
