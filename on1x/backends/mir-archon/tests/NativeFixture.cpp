#include "Bridge.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
namespace m = on1x::mir;
namespace a = on1x::backends::archon;
using on1x::Type;
const on1x::Location at{"native-phi.mir", 0, 1, 1};

void require(bool ok, const std::string &message) {
    if (!ok) throw std::runtime_error(message);
}
std::string read(const std::string &path) {
    std::ifstream in(path);
    require(bool(in), "cannot read " + path);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void write(const std::filesystem::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
    require(bool(out), "cannot write " + path.string());
}
m::Module parse(const std::string &source) {
    auto unit = on1x::parse(on1x::scan(source, "native-source.on1x"));
    on1x::check(unit);
    return m::lower(unit);
}

// Independently authored CFG: both arms of the entry branch choose the same
// successor with different tuple arguments. The self-edge swaps two immutable
// aggregate phis simultaneously. Empty tuples are rejected by canonical types.
m::Module phi(bool nested, bool choose_first) {
    const Type pair = Type::tuple({Type::I64, Type::I64});
    const Type type = nested ? Type::tuple({pair, pair, Type::Bool}) : pair;
    m::Module module;
    module.version = m::format_version;
    m::Function fn;
    fn.id = 2; fn.name = "main"; fn.result = Type::I64; fn.where = at;
    m::Block entry;
    entry.id = 3; entry.effect = 4; entry.where = at;
    entry.instructions = {
        {5, Type::I64, m::Op::Constant, {}, 10, 0, 0, 0, at},
        {6, Type::I64, m::Op::Constant, {}, 20, 0, 0, 0, at},
        {7, Type::Bool, m::Op::Constant, {}, 1, 0, 0, 0, at},
        {8, pair, m::Op::Tuple, {5, 6}, 0, 0, 0, 0, at},
        {9, pair, m::Op::Tuple, {6, 5}, 0, 0, 0, 0, at},
        {31, Type::Bool, m::Op::Constant, {}, choose_first ? 1 : 0, 0, 0, 0, at},
        {34, Type::I64, m::Op::Constant, {}, 5, 0, 0, 0, at}
    };
    m::Id first = 8, second = 9;
    if (nested) {
        entry.instructions.push_back({32, type, m::Op::Tuple, {8, 9, 7}, 0, 0, 0, 0, at});
        entry.instructions.push_back({33, type, m::Op::Tuple, {9, 8, 7}, 0, 0, 0, 0, at});
        first = 32; second = 33;
    }
    entry.exit = {10, m::Exit::Branch, 31, 4,
                  {{11, {first, second, 34}}, {11, {second, first, 34}}}, at};
    m::Block loop;
    loop.id = 11; loop.effect = 12; loop.where = at;
    loop.parameters = {{13, type, at}, {14, type, at}, {15, Type::I64, at}};
    loop.instructions = {
        {16, Type::I64, m::Op::Constant, {}, 0, 0, 0, 0, at},
        {35, Type::I64, m::Op::Constant, {}, 1, 0, 0, 0, at},
        {36, Type::Bool, m::Op::Gt, {15, 16}, 0, 0, 0, 0, at},
        {37, Type::I64, m::Op::Sub, {15, 35}, 0, 0, 12, 38, at}
    };
    loop.exit = {17, m::Exit::Branch, 36, 38,
                 {{11, {14, 13, 37}}, {18, {13, 14}}}, at};
    m::Block done;
    done.id = 18; done.effect = 19; done.where = at;
    done.parameters = {{20, type, at}, {21, type, at}};
    m::Id id = 40, token = done.effect, result = 0;
    {
        for (m::Id tuple : {m::Id{20}, m::Id{21}}) {
            if (nested) {
                const auto value = id++;
                done.instructions.push_back({value, pair, m::Op::Extract, {tuple}, 0, 0, 0, 0, at});
                tuple = value;
            }
            for (int field = 0; field != 2; ++field) {
                const auto value = id++;
                done.instructions.push_back({value, Type::I64, m::Op::Extract, {tuple}, field, 0, 0, 0, at});
                if (!result) result = value;
                const auto print = id++, next = id++;
                done.instructions.push_back({print, Type::Void, m::Op::Print, {value}, 0, 0, token, next, at});
                token = next;
            }
        }
    }
    done.exit = {id++, m::Exit::Return, result, token, {}, at};
    fn.blocks = {entry, done, loop}; // Deliberately not CFG traversal order.
    module.functions.push_back(std::move(fn));
    m::verify(module);
    return module;
}

template<class F> void rejected(const std::string &name, F operation,
                                const std::string &diagnostic = {}) {
    try { operation(); }
    catch (const std::exception &e) {
        require(std::string(e.what()).find(diagnostic) != std::string::npos,
                name + ": unexpected diagnostic: " + e.what());
        std::cout << "PASS " << name << ": " << e.what() << '\n';
        return;
    }
    throw std::runtime_error(name + ": unexpectedly accepted");
}
struct Cancellation {
    std::size_t calls = 0, stop = SIZE_MAX;
    static int check(void *raw) {
        auto &self = *static_cast<Cancellation *>(raw);
        return ++self.calls >= self.stop;
    }
};
void checks() {
    const auto simple = parse("fn main(){var n=0;while n<4{println(n);n=n+1;}}");
    auto reject_options = [&](const char *name, const a::Options &options) {
        a::Product product{"sentinel", "sentinel"};
        rejected(name, [&] { product = a::compile(simple, options); });
        require(product.assembly == "sentinel", "partial product on failure");
    };
    a::Options options;
    options.max_assembly_bytes = 1; reject_options("assembly-budget", options);
    options = {}; options.max_values_per_function = 1; reject_options("value-budget", options);
    options = {}; options.max_instructions_per_function = 1; reject_options("instruction-budget", options);
    options = {}; options.max_instructions_per_module = 1; reject_options("module-instruction-budget", options);
    options = {}; options.max_local_bytes_per_function = 1; reject_options("local-budget", options);
    options = {}; options.verification_limits.nodes = 1; reject_options("verifier-nodes", options);
    options = {}; options.verification_limits.work = 1; reject_options("verifier-work", options);
    auto bad = simple;
    bad.version = 999;
    rejected("malformed-version", [&] { (void)a::compile(bad); }, "version");
    bad = simple; bad.functions[0].blocks[0].exit.effect = 0;
    rejected("malformed-effects", [&] { (void)a::compile(bad); }, "effect");
    bad = simple; bad.functions[0].blocks[0].instructions[0].id = bad.id;
    rejected("malformed-identity", [&] { (void)a::compile(bad); }, "identity");
    bad = parse("fn main(){println(1.0f32);}");
    bad.functions[0].blocks[0].instructions[0].immediate = INT64_C(0x7fc00001);
    rejected("malformed-nan", [&] { (void)a::compile(bad); }, "NaN");
    for (const auto &wire : {"", "(on1x.mir 999 1 ())", "(on1x.mir 1 -1 ())",
                             "(on1x.mir 1 1 ()) trailing"})
        rejected("malformed-wire", [&] { (void)m::deserialize(wire); });
    rejected("unsupported-empty-tuple-type", [] { (void)Type::tuple({}); }, "at least two");
    rejected("unsupported-singleton-tuple-type", [] { (void)Type::tuple({Type::I64}); }, "at least two");
    rejected("unsupported-empty-tuple-source", [] { (void)parse("fn main(){let x=();}"); });

    Cancellation cancellation;
    options = {}; options.cancelled = Cancellation::check;
    options.cancel_userdata = &cancellation;
    require(!a::compile(simple, options).assembly.empty(), "baseline cancellation probe failed");
    const auto checkpoints = cancellation.calls;
    require(checkpoints != 0, "native backend never checked cancellation");
    for (std::size_t stop = 1; stop <= checkpoints; ++stop) {
        cancellation = {0, stop};
        reject_options(("cancellation-" + std::to_string(stop)).c_str(), options);
        require(cancellation.calls >= stop, "cancellation callback not reached");
    }

    // Keep bridge budgets valid while overflowing the generated target's fixed
    // native frame: 32KiB explicit locals plus thousands of SSA spill words.
    auto frame = parse("fn main(){}");
    auto &fn = frame.functions[0];
    auto &block = fn.blocks[0];
    m::Id id = 100;
    for (int n = 0; n < 4096; ++n) fn.slots.push_back({id++, Type::I64, at});
    for (int n = 0; n < 4096; ++n)
        block.instructions.push_back({id++, Type::I64, m::Op::Constant, {}, n, 0, 0, 0, at});
    m::verify(frame);
    (void)a::lower(frame);
    rejected("generated-native-frame", [&] { (void)a::compile(frame); }, "stack frame exceeds");
    require(m::evaluate(simple, "main", {}, 1).status == m::Status::BudgetExhausted,
            "evaluator step budget failed");
    const auto recursion = parse("fn again(){again();}fn main(){again();}");
    require(m::evaluate(recursion, "main", {}, 1000, 8).status == m::Status::BudgetExhausted,
            "evaluator call depth failed");
    std::cout << "PASS all rejection/publication checks; cancellation checkpoints="
              << checkpoints << '\n';
}

void emit(const m::Module &module, const std::filesystem::path &base) {
    const auto wire = m::serialize(module);
    const auto decoded = m::deserialize(wire);
    require(m::structurally_equal(module, decoded), "MIR round-trip mismatch");
    const auto evaluation = m::evaluate(decoded);
    require(evaluation.status != m::Status::BudgetExhausted, "fixture exceeded evaluator budget");
    write(base.string() + ".mir", wire);
    write(base.string() + ".c", m::emit_c(decoded));
    write(base.string() + ".stdout", evaluation.output);
    write(base.string() + ".status", evaluation.status == m::Status::Trapped ? "trap\n" :
          std::to_string(static_cast<unsigned>(evaluation.value) & 255u) + "\n");
    write(base.string() + ".diagnostic", evaluation.diagnostic);
    const auto product = a::compile(decoded);
    require(product.target.find("AMD64") != std::string::npos, "requires actual AMD64 generated backend");
    require(!product.assembly.empty(), "empty native assembly");
    write(base.string() + ".s", product.assembly);
    write(base.string() + ".target", product.target);
}
} // namespace

int main(int argc, char **argv) try {
    if (argc == 2 && std::string(argv[1]) == "--checks") { checks(); return 0; }
    require(argc == 4, "usage: NativeFixture (--source FILE | --phi NAME) OUTPUT_BASE");
    if (std::string(argv[1]) == "--source") emit(parse(read(argv[2])), argv[3]);
    else {
        require(std::string(argv[1]) == "--phi", "unknown mode");
        const std::string name = argv[2];
        emit(phi(name.find("nested") != std::string::npos,
                 name.find("false") == std::string::npos), argv[3]);
    }
    return 0;
} catch (const std::exception &e) {
    std::cerr << "native fixture: " << e.what() << '\n';
    return 1;
}
