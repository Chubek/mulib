#pragma once
#include <on1x/ast.hpp>
namespace on1x {
bool needs_staging(const Unit& unit);
// Internal AOT lowering. infer_types performs this on its transactional clone.
void lower_stages(Unit& unit);
}
