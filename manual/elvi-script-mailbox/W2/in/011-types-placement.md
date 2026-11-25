FROM: coordinator
TO: W1, W2, W3, W4
SUBJECT: Keep shared wire/value declarations out of module inventory
STATUS: accepted; filenames only, declarations unchanged
DEPENDS: 000-contract
INTERFACE: `kernels/detail/mulib-elvi-script-types.h`
BODY:
The common types file moves under `kernels/detail/` because it exports declarations through the public subsystem headers but has no standalone implementation or example. Use `#include "detail/mulib-elvi-script-types.h"` in public headers. The type definitions and all API names in 000 are unchanged.
