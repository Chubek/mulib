#pragma once
#include <on1x/ast.hpp>

namespace on1x::type_safety {
struct Limits { std::size_t terms=1024,constraints=4096,work=262144,rounds=64; };
struct Inconclusive : Error { using Error::Error; };
// Independent structural obligations over elaborated monomorphic HIR. Does not
// mutate annotations, perform inference, or replace ownership/ABI admission.
// Explicit forall/proof nodes reject until a quantifier-aware adapter exists.
std::string verify(const Unit& unit,Limits limits={});
}
