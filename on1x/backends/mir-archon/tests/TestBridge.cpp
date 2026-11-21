#include "Bridge.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace m = on1x::mir;
namespace a = on1x::backends::archon;

m::Module parse(const std::string &source) {
    auto unit = on1x::parse(on1x::scan(source, "archon-test.on1x"));
    on1x::check(unit);
    return m::lower(unit);
}
void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
void compile_fixture(std::string_view expected_name, const std::string &source,
                     unsigned expected_version) {
    const auto program = parse(source);
    require(program.version == expected_version, "unexpected On1x MIR version");
    const auto translated = a::lower(program);
    const auto &view = translated.view();
    require(view.version == ARCHON_MIR_SCHEMA_VERSION, "Archon MIR schema mismatch");
    require(view.function_count == program.functions.size() + 1, "lost native function or entrypoint");
    require(view.global_count == 0, "unexpected native global");
    for (std::size_t n = 0; n < view.function_count; ++n) {
        const auto &f = view.functions[n];
        require(f.name && f.blocks && f.block_count, "missing translated native function");
        require(f.blocks[0].param_count == 0, "entry parameters incorrectly modeled as phi nodes");
    }
    require(std::string(view.functions[view.function_count - 1].name) == "main",
            "missing C-compatible main entrypoint");
    const auto first = a::compile(program);
    require(!first.assembly.empty(), "empty Archon assembly");
    require(first.assembly.find(expected_name) != std::string::npos,
            "generated assembly missing expected function symbol");
    const auto again = a::compile(program);
    require(again.assembly == first.assembly, "nondeterministic assembly");
}
} // namespace

int main() {
    try {
        compile_fixture("on1x_mir_f",
            "fn twice(x:i64)->i64{return x*2;}"
            "fn main(){var n=0;while n<4{println(twice(n));n=n+1;}assert(n==4);}",
            1);
        compile_fixture("on1x_mir_f",
            "fn flip((x,b):(f64,bool))->(bool,f64){return(b,-x);}"
            "fn main(){let(x,y)=flip((1.5,true));println(x);println(y);}",
            2);
        compile_fixture("on1x_mir_f",
            "fn small(x:u8)->u8{return x;}"
            "fn single(x:f32)->f32{return x;}"
            "fn main(){println(255u8);println(1.5f32+2f32);}",
            3);
        {
            auto program = parse("fn main(){println(123);}");
            auto without = a::Options{};
            without.emit_c_entrypoint = false;
            const auto native = a::lower(program, without);
            require(native.view().function_count == program.functions.size(),
                    "library mode unexpectedly emitted main");
            auto bounded = a::Options{};
            bounded.max_assembly_bytes = 0;
            bool rejected = false;
            try { (void)a::compile(program, bounded); }
            catch (const a::Error &) { rejected = true; }
            require(rejected, "zero output budget not rejected");
        }
        std::cout << "on1x.archon: MIR 1/2/3 lowering and deterministic native codegen OK\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "on1x.archon: " << e.what() << '\n';
        return 1;
    }
}
