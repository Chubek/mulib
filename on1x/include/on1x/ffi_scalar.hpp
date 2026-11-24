#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <span>

namespace on1x::ffi {
// Compiler-owned immutable tables describe representation only, not admission.
struct ScalarBinding {
    std::string_view module, provider, header, member, symbol;
    std::size_t arity;
    bool is_inline;
    int guard_lhs, guard_rhs; // -1/-1: no caller-side guard
};
std::span<const ScalarBinding> scalar_bindings() noexcept;
const ScalarBinding* find_scalar(std::string_view symbol) noexcept;
const ScalarBinding* find_member(std::string_view module, std::string_view member) noexcept;
std::string scalar_header(const ScalarBinding& binding);
std::string scalar_prototype(const ScalarBinding& binding, std::string_view emitted_name);
std::string scalar_wrapper(const ScalarBinding& binding, std::string_view emitted_name);
std::string scalar_guard(const ScalarBinding& binding, std::span<const std::string> evaluated_args);
}
