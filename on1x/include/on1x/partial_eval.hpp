#pragma once
#include <on1x/ast.hpp>

namespace on1x::processor {
struct PartialEvalStats {
 bool enabled=false, limited=false;
 std::size_t work=0, folds=0, static_calls=0, residual_calls=0;
};
// candidate is the pipeline's private, already checked HIR. Read definitions
// from its immutable source so specialization is independent of traversal order.
PartialEvalStats partial_evaluate(Unit& candidate,const Unit& source,std::size_t work=262144);
}
