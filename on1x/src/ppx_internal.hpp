#pragma once
#include <on1x/meta.hpp>
namespace on1x::meta {
// Reconstruct syntactic PPX output. The host keeps existing bindings and
// interfaces; new declarations get compiler-created entry-scope bindings.
Unit decode_rewrite(const SExpr& tree, Unit& original);
}
