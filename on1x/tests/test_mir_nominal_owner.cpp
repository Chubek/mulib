#include <on1x/backend/mir_c11.hpp>
#include <on1x/backend/mir_directives.hpp>
#include <on1x/backend/mir_nominal_owner.hpp>
#include <on1x/backend/passes.hpp>
#include <on1x/backend/vm.hpp>

#ifdef ON1X_TEST_OWNER_SDK
#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Context.h"
#include "On1xSDK-IRSerialize.h"
#include "ProgramInternal.hpp"
#include <memory>
#endif
#ifdef ON1X_TEST_OWNER_ARCHON
#include "Bridge.hpp"
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
using namespace on1x;
namespace m = mir;
using m::Id;
using m::Op;

void require(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}

Location at(Id id) { return {"nominal-owner-fixture", id, id, 1}; }

m::Instruction inst(Id id, Op op, Type type, std::vector<Id> args,
                    std::int64_t imm=0, Id target=0, Id in=0, Id out=0) {
    return {id, type, op, std::move(args), imm, target, in, out, at(id)};
}

m::Module fixture() {
    m::Module result;
    result.version = m::nominal_owner_format_version;
    result.nominal_owners = {{30, "example.Counter", "units", Type::I64, at(30)},
                             {31, "example.Weight", "delta", Type::I64, at(31)}};
    result.functions.push_back({2, "main", Type::I64, at(2), {}, {
        {3, 4, at(3), {}, {
            inst(5, Op::Constant, Type::I64, {}, 6),
            inst(6, Op::Constant, Type::I64, {}, 7),
            inst(7, Op::Mul, Type::I64, {5, 6}, 0, 0, 4, 8),
            inst(9, Op::Print, Type::Void, {7}, 0, 0, 8, 10),
            inst(11, Op::OwnerCreate, Type::Void, {7}, 0, 30, 10, 12),
            inst(13, Op::OwnerMove, Type::Void, {11}, 0, 30, 12, 14),
            inst(15, Op::OwnerMove, Type::Void, {13}, 0, 30, 14, 16),
            inst(17, Op::OwnerDrop, Type::Void, {15}, 0, 0, 16, 18),
            inst(19, Op::Constant, Type::I64, {}, -9),
            inst(20, Op::OwnerCreate, Type::Void, {19}, 0, 31, 18, 21),
            inst(22, Op::OwnerDrop, Type::Void, {20}, 0, 0, 21, 23),
            inst(24, Op::Constant, Type::I64, {}, 8),
            inst(25, Op::Sub, Type::I64, {7, 24}, 0, 0, 23, 26),
            inst(27, Op::Print, Type::Void, {25}, 0, 0, 26, 28),
        }, {29, m::Exit::Return, 25, 28, {}, at(29)}}}});
    return result;
}

m::Block& block(m::Module& module) { return module.functions.front().blocks.front(); }

template<class Call>
void reject(std::string_view label, Call action, std::string_view diagnostic, Id site=0) {
    try {
        action();
    } catch (const Error& error) {
        require(std::string_view(error.what()).find(diagnostic) != std::string_view::npos,
                std::string(label) + ": wrong diagnostic: " + error.what());
        if (site)
            require(error.where.file == at(site).file && error.where.line == site &&
                    error.where.column == 1 && error.where.offset == site,
                    std::string(label) + ": wrong source location");
        return;
    }
    throw std::runtime_error(std::string(label) + ": unexpectedly admitted");
}

template<class Edit>
void malformed(std::string_view label, Id site, Edit edit) {
    auto candidate = fixture();
    edit(candidate);
    reject(label, [&]{ m::verify_nominal_owner(candidate); }, "ON1X-MIR-VERIFY", site);
    bool observed = false;
    reject(label, [&]{ (void)m::evaluate_nominal_owner(candidate, "main", {}, 100,
                    [&](const auto&){ observed = true; }); }, "ON1X-MIR-VERIFY", site);
    require(!observed, "malformed input reached evaluator effects before verification");
    reject(label, [&]{ (void)m::emit_nominal_owner_c(candidate, true); }, "ON1X-MIR-VERIFY", site);
}

void malformed_cases() {
    malformed("same-layout nominal substitution", 13, [](auto& m){ block(m).instructions[5].target = 31; });
    malformed("move from stale place", 15, [](auto& m){ block(m).instructions[6].operands[0] = 11; });
    malformed("drop from stale place", 17, [](auto& m){ block(m).instructions[7].operands[0] = 13; });
    malformed("ordinary instruction during live interval", 40, [](auto& m){
        auto& ops = block(m).instructions;
        ops.insert(ops.begin()+5, inst(40, Op::Constant, Type::I64, {}, 10));
    });
    malformed("chained create while live", 13, [](auto& m){
        block(m).instructions[5] = inst(13, Op::OwnerCreate, Type::Void, {7}, 0, 31, 12, 14);
    });
    malformed("drop with value result", 17, [](auto& m){ block(m).instructions[7].type = Type::I64; });
    malformed("ordinary print of a moved place", 27, [](auto& m){ block(m).instructions.back().operands[0] = 15; });
    malformed("broken move effect chain", 13, [](auto& m){ block(m).instructions[5].effect_in = 10; });
    malformed("active owner at return", 29, [](auto& m){
        auto& b = block(m);
        b.instructions.resize(7);
        b.exit.value = 7; b.exit.effect = 16;
    });
    malformed("unsupported CFG", 0, [](auto& m){
        block(m).exit = {29, m::Exit::Jump, 0, 28, {{40, {}}}, at(29)};
        m.functions.front().blocks.push_back(
            {40, 41, at(40), {}, {}, {42, m::Exit::Return, 25, 41, {}, at(42)}});
    });
}

m::Module legacy_scalar() {
    m::Module module;
    module.functions.push_back({2, "main", Type::I64, at(2), {}, {{
        3, 4, at(3), {}, {inst(5, Op::Constant, Type::I64, {}, 42)},
        {6, m::Exit::Return, 5, 4, {}, at(6)}}}});
    return module;
}

#ifdef ON1X_TEST_OWNER_SDK
void sdk_rejections(const m::Module& good, const m::Module& owners,
                    const m::Module& downgraded_ops, const m::Module& downgraded_table,
                    const std::string& good_wire, const std::string& owner_wire,
                    const std::string& downgraded_wire) {
    On1xSDK_Program* wrapped = nullptr;
    require(sdk::make_builtin_mir(good, &wrapped) == ON1X_SDK_OK && wrapped,
            "SDK positive scalar program wrapper");
    on1x_sdk_program_destroy(wrapped);
    for (const auto* rejected : {&owners, &downgraded_ops, &downgraded_table}) {
        wrapped = nullptr;
        require(sdk::make_builtin_mir(*rejected, &wrapped) == ON1X_SDK_UNSUPPORTED && !wrapped,
                "SDK wrapped owner MIR as VERIFIED_MIR");
    }

    On1xSDK_ContextConfig config{};
    config.abi_version = ON1X_SDK_ABI_VERSION_V2;
    config.struct_size = sizeof config;
    On1xSDK_Context* raw = nullptr;
    require(on1x_sdk_context_create(&config, &raw) == ON1X_SDK_OK, "SDK context setup");
    const std::unique_ptr<On1xSDK_Context, decltype(&on1x_sdk_context_release)>
        owner(raw, on1x_sdk_context_release);
    On1xSDK_IRDecodeOptions options{};
    options.abi_version = ON1X_SDK_SEMANTICS_IR_ABI_VERSION;
    options.struct_size = sizeof options;
    options.identity = {ON1X_SDK_SEMANTICS_IR_ABI_VERSION, sizeof options.identity, raw, 1, 1,
                        ON1X_SDK_IR_CANDIDATE, ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION};
    const auto decode = [&](const std::string& wire, On1xSDK_MIRSnapshot** out, On1xSDK_IRFailure* failure) {
        return on1x_sdk_mir_deserialize(wire.data(), wire.size(),
            ON1X_SDK_IR_WIRE_MIR_CANONICAL, &options, out, failure, nullptr);
    };
    On1xSDK_MIRSnapshot* out = nullptr;
    require(decode(good_wire, &out, nullptr) == ON1X_SDK_OK && out, "SDK positive scalar decoder");
    on1x_sdk_mir_snapshot_release(out);
    for (const auto* wire : {&owner_wire, &downgraded_wire}) {
        out = nullptr;
        auto failure = ON1X_SDK_IR_FAILURE_NONE;
        const auto status = decode(*wire, &out, &failure);
        require(status == (wire == &owner_wire ? ON1X_SDK_UNSUPPORTED : ON1X_SDK_ERROR) &&
                failure == ON1X_SDK_IR_FAILURE_MALFORMED && !out, "SDK wire admitted owner MIR");
    }
    std::cout << "SDK public decoder and private program wrapper rejected owner MIR\n";
}
#endif

void legacy_rejections(const m::Module& owners) {
    const auto good = legacy_scalar();
    const auto wire = m::serialize(good);
    require(m::structurally_equal(good, m::deserialize(wire)), "legacy v1 round trip");
    const auto artifact = vm::compile(good);
    require(vm::run_main(artifact).value == 42, "legacy VM positive control");
    reject("owner evaluator does not opt in scalar", [&]{ (void)m::evaluate_nominal_owner(good); }, "ON1X-MIR-VERIFY");
    reject("owner emitter does not opt in scalar", [&]{ (void)m::emit_nominal_owner_c(good); }, "ON1X-MIR-VERIFY");
    reject("legacy evaluate", [&]{ (void)m::evaluate(owners); }, "ON1X-MIR-VERIFY");
    reject("legacy evaluate_values", [&]{ (void)m::evaluate_values(owners, "main", {}); }, "ON1X-MIR-VERIFY");
    reject("legacy emit_c", [&]{ (void)m::emit_c(owners); }, "ON1X-MIR-VERIFY");
    reject("legacy directed C", [&]{ (void)m::emit_directed_c(owners, {}, "test", true); }, "ON1X-MIR-VERIFY");
    reject("legacy serialize", [&]{ (void)m::serialize(owners); }, "ON1X-MIR-VERIFY");
    reject("legacy cfg", [&]{ (void)m::cfg_dot(owners); }, "ON1X-MIR-VERIFY");
    reject("legacy equality", [&]{ (void)m::structurally_equal(good, owners); }, "ON1X-MIR-VERIFY");
    reject("legacy pass", [&]{ (void)m::passes::run(owners); }, "ON1X-MIR-VERIFY");
    reject("legacy transition", [&]{ m::passes::verify_transition(good, owners, m::passes::Pass::DeadPure); },
           "ON1X-MIR-VERIFY");
    reject("legacy VM encoder", [&]{ (void)vm::compile(owners); }, "unsupported executable MIR version");

    auto downgraded_ops = owners;
    downgraded_ops.nominal_owners.clear();
    downgraded_ops.version = m::numeric_format_version;
    auto downgraded_table = good;
    downgraded_table.nominal_owners = owners.nominal_owners;
    for (const auto* rejected : {&downgraded_ops, &downgraded_table}) {
        reject("legacy serialize with disguised owner", [&]{ (void)m::serialize(*rejected); }, "ON1X-MIR-VERIFY");
        reject("legacy pass with disguised owner", [&]{ (void)m::passes::run(*rejected); }, "ON1X-MIR-VERIFY");
        reject("legacy VM encoder with disguised owner", [&]{ (void)vm::compile(*rejected); }, "ON1X-MIR-VERIFY");
    }

    auto future_vm = artifact;
    future_vm.at(std::string_view("O1VM\x01\r\n").size()) = static_cast<char>(m::nominal_owner_format_version);
    reject("legacy VM decoder with tagged owner version", [&]{ vm::verify(future_vm); },
           "unsupported executable MIR version");
    auto owner_wire = wire;
    owner_wire.replace(wire.find("on1x.mir 1"), std::string_view("on1x.mir 1").size(), "on1x.mir 5");
    auto downgraded_wire = wire;
    downgraded_wire.replace(wire.find(" const "), std::string_view(" const ").size(), " owner.create ");
    reject("legacy text v5", [&]{ (void)m::deserialize(owner_wire); }, "unsupported format version");
    reject("legacy text with forged owner opcode", [&]{ (void)m::deserialize(downgraded_wire); }, "unknown opcode");
#ifdef ON1X_TEST_OWNER_SDK
    sdk_rejections(good, owners, downgraded_ops, downgraded_table, wire, owner_wire, downgraded_wire);
#endif
#ifdef ON1X_TEST_OWNER_ARCHON
    require(!backends::archon::compile(good).assembly.empty(), "Archon positive scalar control");
    for (const auto* rejected : {&owners, &std::as_const(downgraded_ops), &std::as_const(downgraded_table)})
        reject("direct Archon compile", [&]{ (void)backends::archon::compile(*rejected); }, "ON1X-MIR-VERIFY");
    std::cout << "direct Archon backend rejected owner MIR before MachineIR lowering\n";
#endif
    std::cout << "legacy evaluator, C, passes, text, CFG and VM refused owner MIR\n";
}

void write(const std::filesystem::path& path, const std::string& body) {
    std::ofstream file(path, std::ios::binary);
    file << body;
    file.close();
    require(file.good(), "failed to write fixture output");
}

void compare_products(const std::filesystem::path& work, const m::Module& module) {
    std::ostringstream trace;
    const auto observe = [&](const m::NominalOwnerEvent& e){
        trace << m::op_name(e.op) << ' ' << e.nominal << ' ' << e.from << ' ' << e.to << ' ' << e.payload << '\n';
    };
    const auto result = m::evaluate_nominal_owner(module, "main", {}, 100, observe);
    require(result.status == m::Status::Returned && result.type == Type::I64 && result.value == 34 &&
            result.output == "42\n34\n" && result.steps == 16, "normal evaluator result");
    write(work/"eval.stdout", result.output);
    write(work/"eval.status", std::to_string(result.value));
    write(work/"eval.trace", trace.str());
    write(work/"plain.c", m::emit_nominal_owner_c(module));
    write(work/"traced.c", m::emit_nominal_owner_c(module, true));

    trace.str("");
    const auto exhausted = m::evaluate_nominal_owner(module, "main", {}, 6, observe);
    require(exhausted.status == m::Status::BudgetExhausted && exhausted.steps == 6 &&
            exhausted.where.line == 13 && exhausted.output == "42\n" &&
            trace.str() == "owner.create 30 0 11 42\n",
            "budget exhaustion executed an implicit move/drop or lost prior output");

    auto trapped = module;
    auto& b = block(trapped);
    b.instructions.push_back(inst(32, Op::Constant, Type::Bool, {}, 0));
    b.instructions.push_back(inst(33, Op::Assert, Type::Void, {32}, 0, 0, 28, 34));
    b.exit.effect = 34;
    trace.str("");
    const auto trap = m::evaluate_nominal_owner(trapped, "main", {}, 100, observe);
    require(trap.status == m::Status::Trapped && trap.where.line == 33 &&
            trap.diagnostic == "assertion failed" && trap.output == result.output,
            "evaluator trap following explicit drop");
    write(work/"trap-eval.stdout", trap.output);
    write(work/"trap-eval.trace", trace.str());
    write(work/"trapped.c", m::emit_nominal_owner_c(trapped, true));
}
}

int main(int argc, char** argv) try {
    require(argc == 2, "expected scratch directory");
    const auto module = fixture();
    m::verify_nominal_owner(module);
    malformed_cases();
    legacy_rejections(module);
    compare_products(argv[1], module);
    std::cout << "nominal-owner evaluator, explicit budgets/traps and malformed admission passed\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
