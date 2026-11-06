#pragma once
#include "printf/Native.hpp"
#include "terminal/Native.hpp"
#include "math/Native.hpp"
#include "text/Native.hpp"
#include "list/Native.hpp"
#ifdef MULL_ENABLE_MULIB
#include "Mulib/Native.hpp"
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
