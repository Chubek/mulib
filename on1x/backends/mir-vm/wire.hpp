#pragma once
#include <on1x/backend/vm.hpp>

namespace on1x::vm {
mir::Module decode(std::string_view bytes);
}
