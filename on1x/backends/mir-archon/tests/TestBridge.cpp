#include "Bridge.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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
template<class F> void rejects(F &&run, ArchonBackendStatus status, const char *message) {
    try { run(); }
    catch (const a::Error &error) {
        require(error.status == status, "wrong structured bridge error status");
        require(!error.where.file.empty(), "bridge error lost source location");
        return;
    }
    throw std::runtime_error(message);
}
struct Cancellation {
    std::size_t polls = 0, stop = 0;
    static int check(void *raw) {
        auto &self = *static_cast<Cancellation *>(raw);
        return ++self.polls >= self.stop;
    }
};
void native_verify(const a::MachineModule &module) {
    ArchonBackendArtifact artifact{};
    ArchonBackendDiagnostic diagnostic{};
    const auto status = archon_backend_compile(&module.view(), nullptr, &artifact, &diagnostic);
    archon_backend_artifact_destroy(&artifact);
    if (status != ARCHON_BACKEND_OK)
        throw std::runtime_error(std::string("real native verifier: ") + diagnostic.message);
}
void boundaries() {
    auto program = parse("fn main(){println(123);}");
    a::Options options;
    Cancellation cancellation{0, 1};
    options.cancelled = Cancellation::check;
    options.cancel_userdata = &cancellation;
    rejects([&] { (void)a::lower(program, options); }, ARCHON_BACKEND_CANCELLED,
            "pre-cancelled lowering ran");
    cancellation = {0, 8};
    rejects([&] { (void)a::lower(program, options); }, ARCHON_BACKEND_CANCELLED,
            "lowering did not poll cancellation");
    cancellation = {0, SIZE_MAX};
    (void)a::lower(program, options);
    const auto lowering_polls = cancellation.polls;
    cancellation = {0, lowering_polls + 1};
    rejects([&] { (void)a::compile(program, options); }, ARCHON_BACKEND_CANCELLED,
            "native-stage cancellation published output");

    options = {};
    options.max_values_per_function = 1;
    rejects([&] { (void)a::lower(parse("fn main(){println(1);println(2);}"), options); },
            ARCHON_BACKEND_LIMIT, "SSA value budget ignored");
    options = {};
    options.max_local_bytes_per_function = 1;
    rejects([&] { (void)a::lower(parse("fn main(){var x=1;println(x);}"), options); },
            ARCHON_BACKEND_LIMIT, "local budget ignored");
    options = {};
    options.max_instructions_per_function = 1;
    rejects([&] { (void)a::lower(parse("fn main(){}"), options); },
            ARCHON_BACKEND_LIMIT, "synthetic entrypoint bypassed function budget");
    options = {};
    options.max_instructions_per_module = 1;
    rejects([&] { (void)a::lower(parse("fn main(){}"), options); },
            ARCHON_BACKEND_LIMIT, "synthetic entrypoint bypassed module budget");
    options.emit_c_entrypoint = false;
    rejects([&] { (void)a::lower(parse(
        "fn first()->i64{return 1;}fn second()->i64{return 2;}fn main(){}"), options); },
            ARCHON_BACKEND_LIMIT, "separate functions bypassed module budget");
    options = {};
    options.max_assembly_bytes = 1;
    rejects([&] { (void)a::compile(program, options); }, ARCHON_BACKEND_LIMIT,
            "native output limit published assembly");

    // Move construction/assignment preserve all borrowed arrays and strings,
    // including imports. Access to a moved-from owner is safely empty.
    auto first = a::lower(program);
    const auto *functions = first.view().functions;
    const auto *imports = first.view().imports;
    auto second = std::move(first);
    require(!first.view().function_count, "moved-from view was not empty");
    auto third = a::lower(parse("fn main(){}"));
    third = std::move(second);
    require(third.view().functions == functions && third.view().imports == imports,
            "move invalidated borrowed MachineIR storage");
    require(!second.view().function_count, "move assignment left a dangling view");
    native_verify(third);
    const auto detached = [] {
        const auto source = parse(
            "fn pair(x:i64)->(i64,i64){return(x,x);}fn main(){let(a,b)=pair(12);println(a);}");
        return a::lower(source);
    }();
    native_verify(detached);

    program.version = 999;
    bool invalid = false;
    try { (void)a::compile(program); }
    catch (const on1x::Error &) { invalid = true; }
    require(invalid, "canonical MIR verifier was bypassed");
}
void parameter_limits() {
    using on1x::Type;
    const on1x::Location at{"abi-limit.on1x", 0, 1, 1};
    auto fixture = [&](std::size_t count, bool tuple) {
        m::Module module;
        module.version = m::format_version;
        m::Function function;
        function.id = 2; function.name = "library"; function.where = at;
        function.result = tuple ? Type::tuple({Type::I64, Type::I64}) : Type{Type::Void};
        m::Block block;
        block.id = 3; block.effect = 4; block.where = at;
        m::Id id = 5;
        for (std::size_t n = 0; n < count; ++n)
            block.parameters.push_back({id++, Type::I64, at});
        m::Id result = 0;
        if (tuple) {
            result = id++;
            block.instructions.push_back({result, function.result, m::Op::Tuple,
                                          {block.parameters[0].id, block.parameters[0].id},
                                          0, 0, 0, 0, at});
        }
        block.exit = {id++, m::Exit::Return, result, block.effect, {}, at};
        function.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(function));
        return module;
    };
    a::Options options;
    options.emit_c_entrypoint = false;
    for (bool tuple : {false, true}) {
        const auto count = ARCHON_MIR_MAX_PARAMS - (tuple ? 1 : 0);
        auto graph = a::lower(fixture(count, tuple), options);
        require(graph.view().functions[0].param_count == ARCHON_MIR_MAX_PARAMS,
                "hidden result pointer counted incorrectly");
        native_verify(graph);
        rejects([&] { (void)a::lower(fixture(count + 1, tuple), options); },
                ARCHON_BACKEND_LIMIT, "native ABI argument limit ignored");
    }
}
void unsigned_operations() {
    std::string source = "fn main(){";
    // The reference evaluator checks actual modular results. The graph checks
    // that lowering uses full-word operations plus explicit narrow truncation.
    for (const std::string type : {"u8", "u16", "u32", "u64"}) {
        const auto max = type + "_from_bits(-1)";
        source += "println(" + type + "_wrapping_add(" + max + ",1" + type + "));";
        source += "println(" + type + "_wrapping_sub(0" + type + ",1" + type + "));";
        source += "println(" + type + "_wrapping_mul(" + max + "," + max + "));";
        source += "println(" + type + "_and(" + max + ",1" + type + "));";
        source += "println(" + type + "_or(0" + type + ",1" + type + "));";
        source += "println(" + type + "_xor(" + max + "," + max + "));";
        source += "println(" + type + "_not(" + max + "));";
    }
    source += "}";
    const auto module = parse(source);
    const auto result = m::evaluate(module);
    require(result.status == m::Status::Returned, "unsigned reference fixture trapped");
    require(result.output ==
        "0\n255\n1\n1\n1\n0\n0\n"
        "0\n65535\n1\n1\n1\n0\n0\n"
        "0\n4294967295\n1\n1\n1\n0\n0\n"
        "0\n18446744073709551615\n1\n1\n1\n0\n0\n", "unsigned modular oracle mismatch");
    const auto graph = a::lower(module);
    for (std::size_t n = 0; n < graph.view().import_count; ++n) {
        const std::string_view symbol(graph.view().imports[n].name);
        require(symbol.find("wrapping_") == symbol.npos &&
                !symbol.ends_with("_and") && !symbol.ends_with("_or") &&
                !symbol.ends_with("_xor") && !symbol.ends_with("_not"),
                "unsigned modular operation still uses a runtime call");
    }
    native_verify(graph);
    require(!a::compile(module).assembly.empty(), "unsigned native emission failed");

    // Overflow/division/negation retain the *On1x* diagnostics and terminal
    // failure, rather than Archon's breakpoint exception.
    const auto checked = a::lower(parse(
        "fn f(x:i64,y:i64)->i64{println(x+y);println(x-y);println(x*y);"
        "println(x/y);println(x%y);return -x;}fn main(){}"));
    for (const auto op : {"add", "sub", "mul", "div", "mod", "neg"}) {
        const auto symbol = "on1x_" + std::string(op) + "_i64";
        bool found = false;
        for (std::size_t n = 0; n < checked.view().import_count; ++n)
            found |= symbol == checked.view().imports[n].name;
        require(found, "checked operation lost exact runtime trap");
    }
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
        boundaries();
        parameter_limits();
        unsigned_operations();
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
