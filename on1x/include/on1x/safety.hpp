#pragma once
#include <on1x/ast.hpp>
#include <chisel/Chisel.hpp>
#include <map>
#include <vector>

namespace on1x::safety {
struct Limits { std::size_t nodes=4096,work=4194304; };
struct BudgetExceeded : Error { using Error::Error; };
// One reviewed scalar/immutable-string foreign call, as recorded in
// on1x.safe-ffi/v1 admission evidence. Void is allowed only as a result.
// This is a claim-backed ABI admission for Chisel's scalar memory model,
// never a semantic statement about the foreign implementation.
struct AdmittedExtern {
 std::vector<Type> parameters; Type result=Type::Void;
 // v4 only: strings are immutable call borrows (parameters) or immutable
 // process-lifetime borrows (result). Never owners or retained input loans.
 bool immutable_strings=false;
};
// Parses line-based v1 (i64/void), v2 (adds bool/f64), v3 (adds u8/u16/u32/u64/f32)
// and v4 (adds directional borrowed_string/static_string) artifacts emitted by
// on1x-bind emit-safe. Rejects malformed, oversized, duplicate or
// out-of-profile entries with located ON1X-ADMISSION errors.
std::map<std::string,AdmittedExtern> parse_admission(const std::string& text);
// Validates a modeled resource CFG, not foreign implementation behavior or ABI.
// Use with token != 0 denotes use through that loan; Use with token == 0 is
// direct owner access. Normal exits must release/transfer all created owners.
// Escape and unknown operations reject. Sources must be real caller locations.
void verify(const moonstone::chisel::Function& function,Limits limits={});
// Checked HIR admission: scalar/string control flow + the trusted JSON facade.
// Rejects raw externs unless each declaration and call site is covered by
// parse_admission evidence with matching admitted ABI; also rejects indirect
// calls and unmodeled managed operations.
// Returns deterministic evidence counts, never a general safe-FFI certificate.
std::string check(const Unit& unit,Limits limits={},const std::map<std::string,AdmittedExtern>& admitted={});
}
