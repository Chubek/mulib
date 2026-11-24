#include <on1x/ffi.hpp>
#include <on1x/ffi_scalar.hpp>
#include <on1x_ffi_rules.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace on1x::ffi {
namespace {
template <typename Key, typename Indices, typename Project>
const Descriptor* lookup(const Key& key, const Indices& order, Project project) noexcept {
    const auto it = std::lower_bound(order.begin(), order.end(), key, [&](std::uint32_t i, const Key& k) {
        return project(generated::bindings[i]) < k;
    });
    if (it == order.end()) return nullptr;
    const auto& d = generated::bindings[*it];
    return project(d) == key ? &d : nullptr;
}

// Compatibility view only. The pre-migration C11 adapters use int64_t and
// accept at most one guard; never coerce another ABI shape into that path.
constexpr bool scalar_compatible(const Descriptor& d) {
    constexpr NativeType native{NativeKind::Integer, "long", {}, 4, 64, 8, 8, true};
    if (d.target.triple != "x86_64-linux-gnu" || d.surface_result != SurfaceKind::I64 ||
        d.native_result != native || d.plan.guards.size() > 1) return false;
    // Keep only the old audited interface in the pre-migration loader when
    // fed v2 data. New declarations must take the typed admission path.
    if (d.schema_version == 2 &&
        !(d.module == "mulib.clamp" && d.placement == Placement::External &&
          ((d.member == "add_i64" && d.symbol == "mulib_clamp_add_i64" &&
            d.parameters.size() == 2 && d.plan.guards.empty()) ||
           (d.member == "sub_i64" && d.symbol == "mulib_clamp_sub_i64" &&
            d.parameters.size() == 2 && d.plan.guards.empty()) ||
           (d.member == "i64" && d.symbol == "mulib_clamp_i64" &&
            d.parameters.size() == 3 && d.plan.guards.size() == 1 &&
            d.plan.guards.front().lhs == 1 && d.plan.guards.front().rhs == 2)))) return false;
    return std::ranges::all_of(d.parameters, [&](const Parameter& p) {
        return p.surface == SurfaceKind::I64 && p.native == native;
    });
}

constexpr auto scalar_view() {
    constexpr auto count = std::ranges::count_if(generated::bindings, scalar_compatible);
    std::array<ScalarBinding, count> values{};
    std::size_t pos = 0;
    for (const auto& d : generated::bindings) {
        if (!scalar_compatible(d)) continue;
        const int lhs = d.plan.guards.empty() ? -1 : d.plan.guards.front().lhs;
        const int rhs = d.plan.guards.empty() ? -1 : d.plan.guards.front().rhs;
        values[pos++] = {d.module, d.provider, d.header, d.member, d.symbol, d.parameters.size(),
                         d.placement == Placement::StaticInline, lhs, rhs};
    }
    return values;
}
constexpr auto scalar_bindings_view = scalar_view();

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

std::span<const Descriptor> foreign_descriptors() noexcept { return generated::bindings; }
const Descriptor* find_descriptor(std::string_view module, std::string_view member) noexcept {
    return lookup(std::pair{module, member}, generated::by_member,
                  [](const Descriptor& d) { return std::pair{d.module, d.member}; });
}
const Descriptor* find_descriptor(ForeignId id) noexcept {
    return lookup(id.sha256, generated::by_id, [](const Descriptor& d) { return d.id.sha256; });
}
const Descriptor* find_descriptor_by_symbol(std::string_view symbol) noexcept {
    return lookup(symbol, generated::by_symbol, [](const Descriptor& d) { return d.symbol; });
}

std::span<const ScalarBinding> scalar_bindings() noexcept { return scalar_bindings_view; }
const ScalarBinding* find_scalar(std::string_view symbol) noexcept {
    for (const auto& b : scalar_bindings()) if (b.symbol == symbol) return &b;
    return nullptr;
}
const ScalarBinding* find_member(std::string_view module, std::string_view member) noexcept {
    for (const auto& b : scalar_bindings()) if (b.module == module && b.member == member) return &b;
    return nullptr;
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
