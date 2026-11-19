#include "On1xSDK-AnalysisManager.h"
#include "On1xSDK-CFG.h"
#include "On1xSDK-Dominator.h"
#include "On1xSDK-EffectAnalysis.h"
#include "On1xSDK-Liveness.h"
#include "On1xSDK-Loop.h"
#include "On1xSDK-PassManager.h"
#include "On1xSDK-TranslationValidation.h"
#include "ProgramInternal.hpp"

#include <on1x/analysis.hpp>
#include <on1x/backend/mir.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); std::abort(); \
} } while (false)

namespace {
namespace m = on1x::mir;

using Program = std::unique_ptr<On1xSDK_Program, decltype(&on1x_sdk_program_destroy)>;

m::Module lower(std::string_view source) {
    auto unit = on1x::parse(on1x::scan(std::string(source), "sdk-analysis.on1x"));
    on1x::check(unit);
    return m::lower(unit);
}

Program program(std::string_view wire) {
    On1xSDK_Program *raw = nullptr;
    CHECK(on1x::sdk::make_verified_mir(wire, &raw) == ON1X_SDK_OK);
    return Program(raw, on1x_sdk_program_destroy);
}

On1xSDK_AnalysisKey key(std::uint64_t revision,
                        std::string_view target = "portable-scalar-v1") {
    return {UINT64_C(0x1234), {UINT64_C(0xaabb), UINT64_C(0xccdd)}, revision,
            {target.data(), target.size()}, UINT64_C(0x55)};
}

template<class T>
T output_record() {
    T value{};
    value.abi_version = ON1X_SDK_ANALYSIS_ABI_VERSION;
    value.struct_size = sizeof(T);
    return value;
}

On1xSDK_Query query_for(std::uint32_t kind, std::uint64_t revision,
                        std::uint64_t work = 4000000,
                        std::uint64_t records = 200000) {
    On1xSDK_Query query{};
    query.abi_version = ON1X_SDK_ANALYSIS_ABI_VERSION;
    query.struct_size = sizeof(query);
    query.source = key(revision);
    query.kind = kind;
    query.analysis_version = ON1X_SDK_ANALYSIS_VERSION;
    query.backend = ON1X_SDK_ANALYSIS_BACKEND_CANONICAL;
    query.control = {ON1X_SDK_ANALYSIS_ABI_VERSION, sizeof(query.control),
                     work, records, nullptr, nullptr};
    return query;
}

enum class StopMode { Continue, Cancel, Deadline, Failure };
struct ControlState {
    StopMode mode = StopMode::Continue;
    std::size_t calls = 0;
};

extern "C" std::uint32_t control(void *userdata, std::uint64_t) {
    auto &state = *static_cast<ControlState *>(userdata);
    ++state.calls;
    switch (state.mode) {
    case StopMode::Continue: return ON1X_SDK_OK;
    case StopMode::Cancel:
        return static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_CANCELLED);
    case StopMode::Deadline:
        return static_cast<std::uint32_t>(ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED);
    case StopMode::Failure: return ON1X_SDK_ERROR;
    }
    return ON1X_SDK_ERROR;
}

struct InstrumentState {
    bool fail = false;
    std::vector<std::tuple<std::uint32_t, std::uint32_t, std::uint64_t>> events;
};

struct ReentryState {
    On1xSDK_AnalysisManager *manager = nullptr;
    const On1xSDK_Query *query = nullptr;
    On1xSDK_Status query_status = ON1X_SDK_ERROR;
    On1xSDK_Status dispose_status = ON1X_SDK_ERROR;
};

extern "C" std::uint32_t reenter(void *userdata, std::uint64_t) {
    auto &state = *static_cast<ReentryState *>(userdata);
    const On1xSDK_AnalysisResult *nested = nullptr;
    state.query_status =
        on1x_sdk_analysis_manager_query(state.manager, state.query, &nested);
    state.dispose_status = on1x_sdk_analysis_manager_dispose(state.manager);
    return ON1X_SDK_OK;
}

extern "C" std::uint32_t instrument(void *userdata,
                                     const On1xSDK_PassEvent *event) {
    auto &state = *static_cast<InstrumentState *>(userdata);
    state.events.emplace_back(event->pass, event->stage, event->round);
    return state.fail && event->stage == ON1X_SDK_PASS_STAGE_AFTER
               ? ON1X_SDK_ERROR
               : ON1X_SDK_OK;
}

On1xSDK_PassRun pass_run(const On1xSDK_Pipeline &pipeline,
                         std::uint64_t revision) {
    On1xSDK_PassRun run{};
    run.abi_version = ON1X_SDK_ANALYSIS_ABI_VERSION;
    run.struct_size = sizeof(run);
    run.source = key(revision);
    run.pipeline = &pipeline;
    run.work_limit = 4194304;
    run.rewrite_limit = 65536;
    run.verifier_node_limit = 65536;
    run.verifier_work_limit = 4194304;
    run.control = {ON1X_SDK_ANALYSIS_ABI_VERSION, sizeof(run.control),
                   run.work_limit, 0, nullptr, nullptr};
    return run;
}

void rethread(m::Block &block) {
    auto token = block.effect;
    for (auto &instruction : block.instructions) {
        if (m::effect(instruction.op) != m::Effect::Pure) {
            instruction.effect_in = token;
            token = instruction.effect_out;
        }
    }
    block.exit.effect = token;
}

m::Module liveness_fixture() {
    const on1x::Location at{"liveness.on1x", 0, 1, 1};
    m::Module module;
    module.id = 1;
    m::Function function;
    function.id = 2;
    function.name = "main";
    function.result = on1x::Type::I64;
    function.where = at;
    m::Block entry;
    entry.id = 3;
    entry.effect = 4;
    entry.where = at;
    entry.instructions.push_back(
        {5, on1x::Type::I64, m::Op::Constant, {}, 42, 0, 0, 0, at});
    entry.exit = {6, m::Exit::Jump, 0, 4, {{7, {5}}}, at};
    m::Block successor;
    successor.id = 7;
    successor.effect = 8;
    successor.where = at;
    successor.parameters.push_back({9, on1x::Type::I64, at});
    successor.exit = {10, m::Exit::Return, 9, 8, {}, at};
    function.blocks = {entry, successor};
    module.functions.push_back(function);
    return module;
}

m::Module irreducible_fixture() {
    const on1x::Location at{"irreducible.on1x", 0, 1, 1};
    m::Module module;
    module.id = 100;
    m::Function function;
    function.id = 101;
    function.name = "main";
    function.where = at;
    m::Block entry;
    entry.id = 102;
    entry.effect = 103;
    entry.where = at;
    entry.instructions.push_back(
        {104, on1x::Type::Bool, m::Op::Constant, {}, 1, 0, 0, 0, at});
    entry.exit = {105, m::Exit::Branch, 104, 103,
                  {{106, {}}, {108, {}}}, at};
    m::Block left;
    left.id = 106;
    left.effect = 107;
    left.where = at;
    left.exit = {110, m::Exit::Jump, 0, 107, {{108, {}}}, at};
    m::Block right;
    right.id = 108;
    right.effect = 109;
    right.where = at;
    right.exit = {111, m::Exit::Jump, 0, 109, {{106, {}}}, at};
    function.blocks = {entry, left, right};
    module.functions.push_back(function);
    return module;
}

m::Module simultaneous_phi_fixture() {
    const on1x::Location at{"simultaneous-phi.on1x", 0, 1, 1};
    m::Module module;
    module.id = 300;
    m::Function function;
    function.id = 301;
    function.name = "main";
    function.result = on1x::Type::I64;
    function.where = at;
    m::Block entry;
    entry.id = 302;
    entry.effect = 303;
    entry.where = at;
    entry.instructions = {
        {304, on1x::Type::I64, m::Op::Constant, {}, 1, 0, 0, 0, at},
        {305, on1x::Type::I64, m::Op::Constant, {}, 2, 0, 0, 0, at},
        {306, on1x::Type::Bool, m::Op::Constant, {}, 1, 0, 0, 0, at},
    };
    entry.exit = {307, m::Exit::Jump, 0, 303,
                  {{308, {304, 305, 306}}}, at};
    m::Block loop;
    loop.id = 308;
    loop.effect = 309;
    loop.where = at;
    loop.parameters = {
        {310, on1x::Type::I64, at},
        {311, on1x::Type::I64, at},
        {312, on1x::Type::Bool, at},
    };
    loop.exit = {313, m::Exit::Branch, 312, 309,
                 {{308, {311, 310, 312}}, {314, {310}}}, at};
    m::Block exit;
    exit.id = 314;
    exit.effect = 315;
    exit.where = at;
    exit.parameters = {{316, on1x::Type::I64, at}};
    exit.exit = {317, m::Exit::Return, 316, 315, {}, at};
    function.blocks = {entry, loop, exit};
    module.functions.push_back(function);
    return module;
}

m::Module numeric_fixture(unsigned version, on1x::Type type,
                          std::int64_t payload) {
    const on1x::Location at{"numeric-payload.on1x", 0, 1, 1};
    m::Module module;
    module.version = version;
    module.id = 400 + version;
    m::Function function;
    function.id = 410 + version;
    function.name = "main";
    function.where = at;
    m::Block block;
    block.id = 420 + version;
    block.effect = 430 + version;
    block.where = at;
    block.instructions = {
        {440 + version, type, m::Op::Constant, {}, payload, 0, 0, 0, at},
        {450 + version, on1x::Type::Void, m::Op::Print,
         {440 + version}, 0, 0, block.effect, 460 + version, at},
    };
    block.exit = {470 + version, m::Exit::Return, 0, 460 + version, {}, at};
    function.blocks.push_back(block);
    module.functions.push_back(function);
    return module;
}

m::Module long_cfg_fixture(std::size_t blocks) {
    const on1x::Location at{"bounded-cfg.on1x", 0, 1, 1};
    m::Module module;
    module.id = 200;
    m::Function function;
    function.id = 201;
    function.name = "main";
    function.where = at;
    std::uint64_t id = 202;
    for (std::size_t index = 0; index < blocks; ++index) {
        m::Block block;
        block.id = id++;
        block.effect = id++;
        block.where = at;
        const auto terminator = id++;
        block.exit = {terminator,
                      index + 1 == blocks ? m::Exit::Return : m::Exit::Jump,
                      0, block.effect, {}, at};
        function.blocks.push_back(block);
    }
    for (std::size_t index = 0; index + 1 < blocks; ++index)
        function.blocks[index].exit.edges.push_back(
            {function.blocks[index + 1].id, {}});
    module.functions.push_back(std::move(function));
    return module;
}

} // namespace

int main(int argc, char **argv) {
    const std::string source_text =
        "fn twice(x:i64)->i64{println(x);return x*2;}"
        "fn main(){var n=0;while n<3{let unused=1==2;"
        "println((n+1)*2);n=n+1;}if true{println(twice(7));}"
        "else{println(1/0);}}";
    auto module = lower(source_text);
    const auto wire = m::serialize(module);
    auto verified = program(wire);

    const auto storage_size = on1x_sdk_analysis_manager_storage_size();
    const auto storage_alignment = on1x_sdk_analysis_manager_storage_alignment();
    void *storage = ::operator new(storage_size, std::align_val_t(storage_alignment));
    On1xSDK_AnalysisManager *manager = nullptr;
    auto config = output_record<On1xSDK_AnalysisManagerConfig>();
    config.owner_token = UINT64_C(0x1234);
    config.max_sources = 4;
    config.max_cache_entries = 8;
    CHECK(on1x_sdk_analysis_manager_initialize(
              storage, storage_size, &config, &manager) == ON1X_SDK_OK);

    auto observed = output_record<On1xSDK_ObservedMIR>();
    observed.key = key(1);
    observed.program = verified.get();
    CHECK(on1x_sdk_analysis_manager_observe(manager, &observed) == ON1X_SDK_OK);

    const On1xSDK_AnalysisResult *cfg = nullptr;
    auto query = query_for(ON1X_SDK_ANALYSIS_CFG, 1);
    CHECK(on1x_sdk_analysis_manager_query(manager, &query, &cfg) == ON1X_SDK_OK);
    CHECK(cfg != nullptr && on1x_sdk_cfg_block_count(manager, cfg) >= 4);
    std::uint64_t previous_function = 0, previous_block = 0;
    for (std::size_t i = 0; i < on1x_sdk_cfg_block_count(manager, cfg); ++i) {
        auto block = output_record<On1xSDK_CFGBlock>();
        CHECK(on1x_sdk_cfg_block_at(manager, cfg, i, &block) == ON1X_SDK_OK);
        CHECK(std::tie(previous_function, previous_block) <=
              std::tie(block.function_id, block.block_id));
        previous_function = block.function_id;
        previous_block = block.block_id;
    }
    auto info = output_record<On1xSDK_AnalysisResultInfo>();
    CHECK(on1x_sdk_analysis_result_info(manager, cfg, &info) == ON1X_SDK_OK);
    CHECK(!info.cache_hit && info.record_count > 0 && info.deterministic_work > 0 &&
          info.backend == ON1X_SDK_ANALYSIS_BACKEND_CANONICAL);
    const On1xSDK_AnalysisResult *cached = nullptr;
    CHECK(on1x_sdk_analysis_manager_query(manager, &query, &cached) == ON1X_SDK_OK);
    CHECK(cached == cfg);
    info = output_record<On1xSDK_AnalysisResultInfo>();
    CHECK(on1x_sdk_analysis_result_info(manager, cached, &info) == ON1X_SDK_OK);
    CHECK(info.cache_hit);
    auto too_small = query;
    too_small.control.work_limit = info.deterministic_work - 1;
    CHECK(on1x_sdk_analysis_manager_query(manager, &too_small, &cached) ==
          ON1X_SDK_LIMIT);
    auto too_few = query;
    too_few.control.result_limit = info.record_count - 1;
    CHECK(on1x_sdk_analysis_manager_query(manager, &too_few, &cached) ==
          ON1X_SDK_LIMIT);
    const On1xSDK_AnalysisResult *other_analysis = nullptr;
    auto first_dominators = query_for(ON1X_SDK_ANALYSIS_DOMINATORS, 1);
    CHECK(on1x_sdk_analysis_manager_query(
              manager, &first_dominators, &other_analysis) == ON1X_SDK_OK);
    info = output_record<On1xSDK_AnalysisResultInfo>();
    CHECK(on1x_sdk_analysis_result_info(manager, cfg, &info) ==
          ON1X_SDK_ANALYSIS_STALE_REVISION);
    CHECK(on1x_sdk_analysis_manager_query(manager, &query, &cached) == ON1X_SDK_OK);
    CHECK(cached == cfg);

    auto wrong_target = query;
    wrong_target.source = key(1, "wrong-target");
    CHECK(on1x_sdk_analysis_manager_query(manager, &wrong_target, &cached) ==
          ON1X_SDK_UNSUPPORTED);
    auto wrong_revision = query;
    wrong_revision.source = key(99);
    CHECK(on1x_sdk_analysis_manager_query(manager, &wrong_revision, &cached) ==
          ON1X_SDK_ANALYSIS_STALE_REVISION);
    auto wrong_owner = query;
    wrong_owner.source.owner_token++;
    CHECK(on1x_sdk_analysis_manager_query(manager, &wrong_owner, &cached) ==
          ON1X_SDK_INVALID_ARGUMENT);
    auto wrong_options = query;
    wrong_options.source.options_key++;
    CHECK(on1x_sdk_analysis_manager_query(manager, &wrong_options, &cached) ==
          ON1X_SDK_UNSUPPORTED);
    auto wrong_version = query;
    wrong_version.analysis_version++;
    CHECK(on1x_sdk_analysis_manager_query(manager, &wrong_version, &cached) ==
          ON1X_SDK_UNSUPPORTED);
    auto alias = query;
    alias.kind = ON1X_SDK_ANALYSIS_ALIAS;
    CHECK(on1x_sdk_analysis_manager_query(manager, &alias, &cached) ==
          ON1X_SDK_UNSUPPORTED);
    auto reserved_query = query;
    reserved_query.reserved = 1;
    CHECK(on1x_sdk_analysis_manager_query(manager, &reserved_query, &cached) ==
          ON1X_SDK_INVALID_ARGUMENT);
    auto novostone = query;
    novostone.backend = ON1X_SDK_ANALYSIS_BACKEND_NOVOSTONE;
#ifdef ON1X_SDK_TEST_WITH_NOVOSTONE
    CHECK(on1x_sdk_analysis_manager_query(manager, &novostone, &cached) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_cfg_block_count(manager, cached) >= 4);
#else
    CHECK(on1x_sdk_analysis_manager_query(manager, &novostone, &cached) ==
          ON1X_SDK_UNSUPPORTED);
#endif
    auto wetstone = query;
    wetstone.backend = ON1X_SDK_ANALYSIS_BACKEND_WETSTONE;
    CHECK(on1x_sdk_analysis_manager_query(manager, &wetstone, &cached) ==
          ON1X_SDK_UNSUPPORTED);

    auto other = lower("fn main(){println(99);}");
    auto other_program = program(m::serialize(other));
    auto conflict = observed;
    conflict.program = other_program.get();
    CHECK(on1x_sdk_analysis_manager_observe(manager, &conflict) ==
          ON1X_SDK_ANALYSIS_REVISION_CONFLICT);

    auto malformed_wire = wire;
    const auto edge = malformed_wire.find("(edge ");
    CHECK(edge != std::string::npos);
    const auto edge_id = edge + std::strlen("(edge ");
    malformed_wire.replace(edge_id, malformed_wire.find(' ', edge_id) - edge_id,
                           "999999999");
    auto malformed_program = program(malformed_wire);
    auto malformed = observed;
    malformed.key = key(2);
    malformed.program = malformed_program.get();
    CHECK(on1x_sdk_analysis_manager_observe(manager, &malformed) ==
          ON1X_SDK_ANALYSIS_VERIFICATION_FAILED);
    CHECK(on1x_sdk_analysis_manager_query(manager, &query, &cached) == ON1X_SDK_OK);
    On1xSDK_Program *raw_source = nullptr;
    CHECK(on1x_sdk_program_create_source(wire.data(), wire.size(), &raw_source) ==
          ON1X_SDK_OK);
    Program source_program(raw_source, on1x_sdk_program_destroy);
    auto wrong_phase = malformed;
    wrong_phase.program = source_program.get();
    CHECK(on1x_sdk_analysis_manager_observe(manager, &wrong_phase) ==
          ON1X_SDK_UNSUPPORTED);

    observed.key = key(2);
    CHECK(on1x_sdk_analysis_manager_observe(manager, &observed) == ON1X_SDK_OK);
    CHECK(on1x_sdk_analysis_manager_query(manager, &query, &cached) ==
          ON1X_SDK_ANALYSIS_STALE_REVISION);

    const On1xSDK_AnalysisResult *result = nullptr;
    auto dominator_query = query_for(ON1X_SDK_ANALYSIS_DOMINATORS, 2);
    CHECK(on1x_sdk_analysis_manager_query(manager, &dominator_query, &result) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_dominator_count(manager, result) >= 4);
    auto entry_dom = output_record<On1xSDK_Dominator>();
    bool found_entry = false;
    for (std::size_t i = 0; i < on1x_sdk_dominator_count(manager, result); ++i) {
        CHECK(on1x_sdk_dominator_at(manager, result, i, &entry_dom) == ON1X_SDK_OK);
        found_entry |= entry_dom.immediate_dominator_id == 0;
    }
    CHECK(found_entry);

    auto loop_query = query_for(ON1X_SDK_ANALYSIS_LOOPS, 2);
    CHECK(on1x_sdk_analysis_manager_query(manager, &loop_query, &result) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_loop_count(manager, result) >= 1);
    CHECK(on1x_sdk_loop_member_count(manager, result) >= 2);
    auto loop = output_record<On1xSDK_Loop>();
    CHECK(on1x_sdk_loop_at(manager, result, 0, &loop) == ON1X_SDK_OK);
    CHECK(loop.is_reducible && loop.header_block_id != 0);

    auto live_query = query_for(ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF, 2);
    CHECK(on1x_sdk_analysis_manager_query(manager, &live_query, &result) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_definition_count(manager, result) > 0);
    CHECK(on1x_sdk_use_count(manager, result) > 0);

    auto effect_query = query_for(ON1X_SDK_ANALYSIS_EFFECTS, 2);
    CHECK(on1x_sdk_analysis_manager_query(manager, &effect_query, &result) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_effect_step_count(manager, result) > 0);
    bool saw_io = false, saw_call = false;
    for (std::size_t i = 0; i < on1x_sdk_function_effect_count(manager, result); ++i) {
        auto effects = output_record<On1xSDK_FunctionEffects>();
        CHECK(on1x_sdk_function_effect_at(manager, result, i, &effects) == ON1X_SDK_OK);
        saw_io |= (effects.transitive_effect_mask & ON1X_SDK_EFFECT_MASK_IO) != 0;
        saw_call |= (effects.transitive_effect_mask & ON1X_SDK_EFFECT_MASK_CALL) != 0;
    }
    CHECK(saw_io && saw_call);

    ReentryState reentry{manager, &effect_query};
    auto reentrant_query = effect_query;
    reentrant_query.control.poll = reenter;
    reentrant_query.control.userdata = &reentry;
    CHECK(on1x_sdk_analysis_manager_query(manager, &reentrant_query, &result) ==
          ON1X_SDK_OK);
    CHECK(reentry.query_status == ON1X_SDK_BUSY &&
          reentry.dispose_status == ON1X_SDK_BUSY);

#ifdef ON1X_SDK_TEST_WITH_NOVOSTONE
    for (auto kind : {ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF,
                      ON1X_SDK_ANALYSIS_EFFECTS}) {
        auto optional = query_for(kind, 2);
        optional.backend = ON1X_SDK_ANALYSIS_BACKEND_NOVOSTONE;
        CHECK(on1x_sdk_analysis_manager_query(manager, &optional, &result) ==
              ON1X_SDK_OK);
        if (kind == ON1X_SDK_ANALYSIS_LIVENESS_USE_DEF)
            CHECK(on1x_sdk_definition_count(manager, result) > 0);
        else
            CHECK(on1x_sdk_function_effect_count(manager, result) > 0);
    }
#endif

    ControlState control_state{StopMode::Cancel};
    auto controlled = effect_query;
    controlled.control.poll = control;
    controlled.control.userdata = &control_state;
    CHECK(on1x_sdk_analysis_manager_query(manager, &controlled, &result) ==
          ON1X_SDK_ANALYSIS_CANCELLED);
    control_state = {StopMode::Deadline};
    CHECK(on1x_sdk_analysis_manager_query(manager, &controlled, &result) ==
          ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED);
    control_state = {StopMode::Failure};
    CHECK(on1x_sdk_analysis_manager_query(manager, &controlled, &result) ==
          ON1X_SDK_ERROR);
    auto zero_budget = effect_query;
    zero_budget.control.work_limit = 0;
    CHECK(on1x_sdk_analysis_manager_query(manager, &zero_budget, &result) ==
          ON1X_SDK_LIMIT);

    auto pipeline = output_record<On1xSDK_Pipeline>();
    constexpr std::string_view pipeline_text =
        " constant-fold, simplify-cfg, local-slots, dead-pure ";
    CHECK(on1x_sdk_pipeline_parse(pipeline_text.data(), pipeline_text.size(), 8,
                                  &pipeline) == ON1X_SDK_OK);
    auto bad_pipeline = output_record<On1xSDK_Pipeline>();
    CHECK(on1x_sdk_pipeline_parse("constant-fold,", 14, 8, &bad_pipeline) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_pipeline_parse("shell", 5, 8, &bad_pipeline) ==
          ON1X_SDK_UNSUPPORTED);
    auto reserved_pipeline = pipeline;
    reserved_pipeline.reserved = 1;

    auto descriptor = output_record<On1xSDK_PassDescriptor>();
    CHECK(on1x_sdk_pass_descriptor(ON1X_SDK_PASS_DEAD_PURE, &descriptor) ==
          ON1X_SDK_OK);
    CHECK((descriptor.preserves_mask & ON1X_SDK_PRESERVE_EFFECTS) != 0);
    CHECK(on1x_sdk_pass_descriptor(
              static_cast<On1xSDK_PassKind>(99), &descriptor) ==
          ON1X_SDK_UNSUPPORTED);

    InstrumentState instrumentation;
    auto run = pass_run(pipeline, 2);
    run.instrument = instrument;
    run.instrument_userdata = &instrumentation;
    const On1xSDK_PassResult *passes = nullptr;
    auto reserved_run = pass_run(reserved_pipeline, 2);
    CHECK(on1x_sdk_pass_manager_run(manager, &reserved_run, &passes) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_pass_manager_run(manager, &run, &passes) == ON1X_SDK_OK);
    CHECK(passes != nullptr && !instrumentation.events.empty());
    auto pass_info = output_record<On1xSDK_PassResultInfo>();
    CHECK(on1x_sdk_pass_result_info(manager, passes, &pass_info) == ON1X_SDK_OK);
    CHECK(pass_info.complete && pass_info.changed && pass_info.event_count > 0);
    CHECK(instrumentation.events.size() == pass_info.event_count * 2);
    for (std::size_t index = 0; index < instrumentation.events.size(); index += 2) {
        CHECK(std::get<1>(instrumentation.events[index]) ==
              ON1X_SDK_PASS_STAGE_BEFORE);
        CHECK(std::get<1>(instrumentation.events[index + 1]) ==
              ON1X_SDK_PASS_STAGE_AFTER);
        CHECK(std::get<0>(instrumentation.events[index]) ==
              std::get<0>(instrumentation.events[index + 1]));
        CHECK(std::get<2>(instrumentation.events[index]) ==
              std::get<2>(instrumentation.events[index + 1]));
    }
    auto recorded_event = output_record<On1xSDK_PassEvent>();
    CHECK(on1x_sdk_pass_result_event_at(manager, passes, 0, &recorded_event) ==
          ON1X_SDK_OK);
    CHECK(recorded_event.stage == ON1X_SDK_PASS_STAGE_AFTER);
    const std::uint8_t *optimized_bytes = nullptr;
    std::size_t optimized_size = 0;
    CHECK(on1x_sdk_pass_result_bytes(manager, passes, &optimized_bytes,
                                     &optimized_size) == ON1X_SDK_OK);
    const std::string optimized_wire(reinterpret_cast<const char *>(optimized_bytes),
                                     optimized_size);
    CHECK(optimized_wire != wire && m::serialize(module) == wire);
    auto optimized = m::deserialize(optimized_wire);
    const auto before_eval = m::evaluate(module);
    const auto after_eval = m::evaluate(optimized);
    CHECK(before_eval.status == after_eval.status &&
          before_eval.output == after_eval.output &&
          before_eval.value == after_eval.value);

    auto test = output_record<On1xSDK_TranslationTest>();
    test.function_name = "main";
    test.function_name_size = 4;
    test.evaluator_step_limit = 1000000;
    test.evaluator_call_depth_limit = 256;
    auto evidence = output_record<On1xSDK_TranslationEvidence>();
    CHECK(on1x_sdk_translation_test(manager, passes, &test, &evidence) ==
          ON1X_SDK_OK);
    CHECK(evidence.evidence == ON1X_SDK_TRANSLATION_EVIDENCED_EQUAL &&
          evidence.tests_executed == 1 && !evidence.is_equivalence_certificate);

    auto mutant = module;
    bool removed = false;
    for (auto &function : mutant.functions) for (auto &block : function.blocks) {
        auto found = std::find_if(block.instructions.begin(), block.instructions.end(),
                                  [](const auto &value) { return value.op == m::Op::Print; });
        if (found != block.instructions.end()) {
            block.instructions.erase(found);
            rethread(block);
            removed = true;
            break;
        }
    }
    CHECK(removed);
    const auto mutant_wire = m::serialize(mutant);
    CHECK(on1x_sdk_pass_verify_transition(
              manager, &run.source, ON1X_SDK_PASS_DEAD_PURE,
              mutant_wire.data(), mutant_wire.size(), 65536, 4194304) ==
          ON1X_SDK_ANALYSIS_VERIFICATION_FAILED);
    CHECK(on1x_sdk_pass_verify_transition(
              manager, &run.source, ON1X_SDK_PASS_DEAD_PURE,
              "bad", 3, 65536, 4194304) ==
          ON1X_SDK_ANALYSIS_VERIFICATION_FAILED);
    CHECK(on1x_sdk_pass_verify_transition(
              manager, &run.source, ON1X_SDK_PASS_DEAD_PURE,
              optimized_wire.data(), optimized_wire.size(), 0, 4194304) ==
          ON1X_SDK_LIMIT);

    const auto source_after_success = wire;
    auto limited_run = run;
    limited_run.instrument = nullptr;
    limited_run.work_limit = 0;
    limited_run.control.work_limit = 0;
    const On1xSDK_PassResult *failed = passes;
    CHECK(on1x_sdk_pass_manager_run(manager, &limited_run, &failed) ==
          ON1X_SDK_LIMIT);
    CHECK(failed == nullptr && m::serialize(module) == source_after_success);
    auto verifier_limited_run = run;
    verifier_limited_run.instrument = nullptr;
    verifier_limited_run.verifier_node_limit = 0;
    CHECK(on1x_sdk_pass_manager_run(manager, &verifier_limited_run, &failed) ==
          ON1X_SDK_LIMIT);
    auto host_limited_run = run;
    host_limited_run.instrument = nullptr;
    host_limited_run.control.work_limit = 0;
    CHECK(on1x_sdk_pass_manager_run(manager, &host_limited_run, &failed) ==
          ON1X_SDK_LIMIT);
    auto invalid_control_run = run;
    invalid_control_run.instrument = nullptr;
    invalid_control_run.control.result_limit = 1;
    CHECK(on1x_sdk_pass_manager_run(manager, &invalid_control_run, &failed) ==
          ON1X_SDK_INVALID_ARGUMENT);

    control_state = {StopMode::Cancel};
    auto stopped_run = run;
    stopped_run.instrument = nullptr;
    stopped_run.control.poll = control;
    stopped_run.control.userdata = &control_state;
    CHECK(on1x_sdk_pass_manager_run(manager, &stopped_run, &failed) ==
          ON1X_SDK_ANALYSIS_CANCELLED);
    control_state = {StopMode::Deadline};
    CHECK(on1x_sdk_pass_manager_run(manager, &stopped_run, &failed) ==
          ON1X_SDK_ANALYSIS_DEADLINE_EXCEEDED);
    control_state = {StopMode::Failure};
    CHECK(on1x_sdk_pass_manager_run(manager, &stopped_run, &failed) ==
          ON1X_SDK_ERROR);
    InstrumentState failed_instrumentation{true, {}};
    auto callback_run = run;
    callback_run.instrument_userdata = &failed_instrumentation;
    CHECK(on1x_sdk_pass_manager_run(manager, &callback_run, &failed) ==
          ON1X_SDK_ERROR);
    CHECK(m::serialize(module) == source_after_success);
    auto one_round = output_record<On1xSDK_Pipeline>();
    CHECK(on1x_sdk_pipeline_parse(
              "constant-fold", 13, 1, &one_round) == ON1X_SDK_OK);
    auto round_limited_run = pass_run(one_round, 2);
    CHECK(on1x_sdk_pass_manager_run(manager, &round_limited_run, &failed) ==
          ON1X_SDK_LIMIT);
    CHECK(failed == nullptr);
    CHECK(on1x_sdk_analysis_manager_query(manager, &effect_query, &result) ==
          ON1X_SDK_OK);
    auto post_pass_cfg = query_for(ON1X_SDK_ANALYSIS_CFG, 2);
    CHECK(on1x_sdk_analysis_manager_query(manager, &post_pass_cfg, &result) ==
          ON1X_SDK_OK);
    pass_info = output_record<On1xSDK_PassResultInfo>();
    CHECK(on1x_sdk_pass_result_info(manager, passes, &pass_info) ==
          ON1X_SDK_ANALYSIS_STALE_REVISION);

    if (argc == 4 && std::string_view(argv[1]) == "--emit-c") {
        std::ofstream before(argv[2]), after(argv[3]);
        before << m::emit_c(module);
        after << m::emit_c(optimized);
        CHECK(before.good() && after.good());
    } else {
        CHECK(argc == 1);
    }

    auto live_options = m::analysis::Options{};
    live_options.kind = m::analysis::Kind::LivenessUseDef;
    live_options.work = 100000;
    live_options.records = 1000;
    auto live_result = m::analysis::run(liveness_fixture(), live_options);
    CHECK(live_result.status == m::analysis::Status::Complete);
    CHECK(live_result.definitions.size() == 2 && live_result.uses.size() == 2);
    CHECK(std::any_of(live_result.liveness.begin(), live_result.liveness.end(),
                      [](const auto &value) {
                          return !value.live_in && value.value == 5;
                      }));
    CHECK(std::none_of(live_result.liveness.begin(), live_result.liveness.end(),
                       [](const auto &value) {
                           return value.live_in && value.value == 9;
                       }));
    auto simultaneous =
        m::analysis::run(simultaneous_phi_fixture(), live_options);
    CHECK(simultaneous.status == m::analysis::Status::Complete);
    CHECK(std::any_of(simultaneous.liveness.begin(),
                      simultaneous.liveness.end(), [](const auto &value) {
                          return value.block == 308 && !value.live_in &&
                                 value.value == 311;
                      }));

    auto loop_options = m::analysis::Options{};
    loop_options.kind = m::analysis::Kind::Loops;
    loop_options.work = 100000;
    loop_options.records = 1000;
    auto irreducible = m::analysis::run(irreducible_fixture(), loop_options);
    CHECK(irreducible.status == m::analysis::Status::Complete);
    CHECK(std::any_of(irreducible.loops.begin(), irreducible.loops.end(),
                      [](const auto &value) {
                          return !value.reducible && value.header == 0 &&
                                 value.latch == 0;
                      }));

    auto bounded_options = m::analysis::Options{};
    bounded_options.kind = m::analysis::Kind::Dominators;
    bounded_options.work = 15000;
    bounded_options.records = 1000;
    auto bounded =
        m::analysis::run(long_cfg_fixture(1024), bounded_options);
    CHECK(bounded.status == m::analysis::Status::BudgetExhausted &&
          bounded.record_count() == 0);

    auto product_module =
        numeric_fixture(m::format_version, on1x::Type::F64,
                        std::numeric_limits<std::int64_t>::min());
    auto product_program = program(m::serialize(product_module));
    auto product_observed = output_record<On1xSDK_ObservedMIR>();
    product_observed.key = key(1, "portable-numeric-product-v2");
    product_observed.program = product_program.get();
    CHECK(on1x_sdk_analysis_manager_observe(manager, &product_observed) ==
          ON1X_SDK_OK);
    auto product_query = query_for(ON1X_SDK_ANALYSIS_CFG, 1);
    product_query.source = product_observed.key;
    CHECK(on1x_sdk_analysis_manager_query(manager, &product_query, &result) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_cfg_block_count(manager, result) == 1);

    auto numeric_module =
        numeric_fixture(m::numeric_format_version, on1x::Type::U64, -1);
    const auto numeric_wire = m::serialize(numeric_module);
    auto numeric_program = program(numeric_wire);
    auto numeric_observed = output_record<On1xSDK_ObservedMIR>();
    numeric_observed.key = key(1, "portable-numeric-v3");
    numeric_observed.program = numeric_program.get();
    CHECK(on1x_sdk_analysis_manager_observe(manager, &numeric_observed) ==
          ON1X_SDK_OK);
    auto numeric_query = query_for(ON1X_SDK_ANALYSIS_EFFECTS, 1);
    numeric_query.source = numeric_observed.key;
    CHECK(on1x_sdk_analysis_manager_query(manager, &numeric_query, &result) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_effect_step_count(manager, result) == 1);
    auto numeric_novostone = numeric_query;
    numeric_novostone.backend = ON1X_SDK_ANALYSIS_BACKEND_NOVOSTONE;
    CHECK(on1x_sdk_analysis_manager_query(
              manager, &numeric_novostone, &result) == ON1X_SDK_UNSUPPORTED);
    auto numeric_run = pass_run(pipeline, 1);
    numeric_run.source = numeric_observed.key;
    CHECK(on1x_sdk_pass_manager_run(manager, &numeric_run, &passes) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_pass_result_bytes(
              manager, passes, &optimized_bytes, &optimized_size) == ON1X_SDK_OK);
    CHECK(std::string_view(reinterpret_cast<const char *>(optimized_bytes),
                           optimized_size) == numeric_wire);

    on1x::Location wide_at{"wide-ids.on1x", 0, 1, 1};
    m::Module wide;
    wide.id = UINT64_MAX;
    m::Function wide_function;
    wide_function.id = UINT64_MAX - 1;
    wide_function.name = "main";
    wide_function.where = wide_at;
    m::Block wide_block;
    wide_block.id = UINT64_MAX - 2;
    wide_block.effect = UINT64_MAX - 3;
    wide_block.where = wide_at;
    wide_block.exit = {UINT64_MAX - 4, m::Exit::Return, 0,
                       wide_block.effect, {}, wide_at};
    wide_function.blocks.push_back(wide_block);
    wide.functions.push_back(wide_function);
    const auto wide_wire = m::serialize(wide);
    auto wide_program = program(wide_wire);
    auto wide_observed = observed;
    wide_observed.key = key(3);
    wide_observed.program = wide_program.get();
    CHECK(on1x_sdk_analysis_manager_observe(manager, &wide_observed) == ON1X_SDK_OK);
    auto wide_query = query_for(ON1X_SDK_ANALYSIS_CFG, 3);
    CHECK(on1x_sdk_analysis_manager_query(manager, &wide_query, &result) ==
          ON1X_SDK_OK);
    auto wide_record = output_record<On1xSDK_CFGBlock>();
    CHECK(on1x_sdk_cfg_block_at(manager, result, 0, &wide_record) == ON1X_SDK_OK);
    CHECK(wide_record.function_id == UINT64_MAX - 1 &&
          wide_record.block_id == UINT64_MAX - 2);

    auto stats = output_record<On1xSDK_AnalysisManagerStats>();
    CHECK(on1x_sdk_analysis_manager_stats(manager, &stats) == ON1X_SDK_OK);
    CHECK(stats.cache_hits >= 2 && stats.cache_misses >= 5);
    CHECK(on1x_sdk_analysis_manager_dispose(manager) == ON1X_SDK_OK);
    ::operator delete(storage, std::align_val_t(storage_alignment));
    std::puts("SDK revision analyses and transactional passes PASS");
}
