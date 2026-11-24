#include <on1x/ffi_scalar.hpp>
#include <on1x_ffi_rules.hpp>
#include <stdexcept>
namespace on1x::ffi {
std::span<const ScalarBinding> scalar_bindings() noexcept { return generated::bindings; }
const ScalarBinding* find_scalar(std::string_view symbol) noexcept {
    for (const auto& b : scalar_bindings()) if (b.symbol == symbol) return &b;
    return nullptr;
}
const ScalarBinding* find_member(std::string_view module, std::string_view member) noexcept {
    for (const auto& b : scalar_bindings()) if (b.module == module && b.member == member) return &b;
    return nullptr;
}
namespace {
std::string args(std::size_t n, bool named, bool types) {
    if (n == 0) return types ? "void" : "";
    std::string out;
    for (std::size_t i=0;i<n;++i) {
        if (i) out += ", ";
        if (types) out += "int64_t";
        if (named) { if(types) out += " "; out += "a"+std::to_string(i); }
    }
    return out;
}
void identifier(std::string_view s) {
    const auto alpha=[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';};
    if(s.empty() || !alpha(s.front())) throw std::invalid_argument("FFIHandler: invalid emitted identifier");
    for(char c:s) if(!alpha(c)&&!(c>='0'&&c<='9')) throw std::invalid_argument("FFIHandler: invalid emitted identifier");
}
}
std::string scalar_header(const ScalarBinding& b) {
    return "#include <"+std::string(b.header)+">\n";
}
std::string scalar_prototype(const ScalarBinding& b,std::string_view name) {
    identifier(name);
    return "extern int64_t "+std::string(name)+"("+args(b.arity,false,true)+");\n";
}
std::string scalar_wrapper(const ScalarBinding& b,std::string_view name) {
    identifier(name);
    const std::string sym(b.symbol), emitted(name);
    return "/* unsafe_foreign: "+sym+" */\n"
        "_Static_assert(_Generic(&"+sym+", int64_t (*)("+args(b.arity,false,true)+"): 1, default: 0), \"KernelBind prototype drift\");\n"
        "static int64_t "+emitted+"("+args(b.arity,true,true)+") { return "+sym+"("+args(b.arity,true,false)+"); }\n";
}
std::string scalar_guard(const ScalarBinding& b,std::span<const std::string> values) {
    if(values.size()!=b.arity) throw std::invalid_argument("FFIHandler: guard argument arity mismatch");
    if(b.guard_lhs==-1&&b.guard_rhs==-1) return {};
    if(b.guard_lhs<0||b.guard_rhs<0||std::size_t(b.guard_lhs)>=values.size()||std::size_t(b.guard_rhs)>=values.size())
        throw std::invalid_argument("FFIHandler: invalid guard index");
    // These are already-evaluated, compiler-generated C temporaries, not DSL text.
    return "on1x_assert("+values[b.guard_lhs]+" <= "+values[b.guard_rhs]+");\n";
}
}
