#pragma once
#include <on1x/ast.hpp>
#include <chisel/Chisel.hpp>

namespace on1x::safety {
struct Limits { std::size_t nodes=4096,work=4194304; };
struct BudgetExceeded : Error { using Error::Error; };
// Validates a modeled resource CFG, not foreign implementation behavior or ABI.
// Use with token != 0 denotes use through that loan; Use with token == 0 is
// direct owner access. Normal exits must release/transfer all created owners.
// Escape and unknown operations reject. Sources must be real caller locations.
void verify(const moonstone::chisel::Function& function,Limits limits={});
// Checked HIR admission: scalar/string control flow + the trusted JSON facade.
// Rejects raw externs, indirect calls and unmodeled managed operations.
// Returns deterministic evidence counts, never a general safe-FFI certificate.
std::string check(const Unit& unit,Limits limits={});
}
