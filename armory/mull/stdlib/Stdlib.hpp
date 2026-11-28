#pragma once
#include "../native/printf/Native.hpp"
#include "../native/terminal/Native.hpp"
#include "../native/math/Native.hpp"
#include "../native/text/Native.hpp"
#include "../native/list/Native.hpp"
#ifdef MULL_ENABLE_MULIB
#include "../bridge/Mulib/Native.hpp"
#endif

namespace mull::stdlib {
inline void install(Runtime &runtime) {
  installPrintf(runtime); installTerminal(runtime); installMath(runtime);
  installText(runtime); installList(runtime);
#ifdef MULL_ENABLE_MULIB
  installMulib(runtime);
#endif
}
}
