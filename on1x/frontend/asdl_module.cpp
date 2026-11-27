#include <on1x/asdl_module.hpp>
#include <algorithm>
#include <span>

namespace on1x {
namespace {
// One beyond the ordinary presentation cap allows its registry to signal
// truncation. This pass remains advisory even if the Lua budget is exceeded.
constexpr std::size_t max_unused_reports=1025;
constexpr const char* unused_group="unused";
struct UnusedPattern {const char* tag;const char* code;const char* name;};
constexpr UnusedPattern unused_pattern(UnusedKind kind){
    switch(kind){
    case UnusedKind::Import: return {"UnusedImport","ON1X-UNUSED-IMPORT","unused-import"};
    case UnusedKind::Variable: return {"UnusedVariable","ON1X-UNUSED-VARIABLE","unused-variable"};
    case UnusedKind::Type: return {"UnusedType","ON1X-UNUSED-TYPE","unused-type"};
    }
    throw std::logic_error("unknown source observation");
}
void native_unused_records(std::span<const UnusedBinding> observations, const diagnostic::Level& level,
                           std::vector<diagnostic::Record>& records) {
    for (const auto& item : observations) {
        const auto pattern=unused_pattern(item.kind);
        const char* prefix="unused ";
        if (item.kind==UnusedKind::Import) prefix="unused import of ";
        if (item.kind==UnusedKind::Type) prefix="unused private ";
        records.push_back({.id=pattern.code,.message=prefix+item.binding_kind+" '"+item.name+"'",
            .category=unused_group,.phase="frontend",.origin="on1x-resolver",.level=level,.where=item.where,
            .name=pattern.name,.group=unused_group});
    }
}
}
}

#ifdef ON1X_ASDL_MODULES
#include <on1x/loader.hpp>
#include <on1x/module_signature.hpp>
#include "asdl_loader.h"
#include <asdl/execution.hpp>
#include <asdl/lua.hpp>
#include <asdl/schema.hpp>
#include "on1x_source_asdl.hpp"
#include "on1x_std_diagnostics.hpp"
#include <string_view>
#include <utility>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace on1x {
namespace {

const asdl::Schema& module_schema() {
    static const auto schema = [] {
        auto result = asdl::parse(source_asdl::schema, "On1x.asdl");
        // push_execution can resolve .import using a filesystem loader. The
        // compiler does not allow this: hooks receive signed handles instead.
        if (!result.imports.empty()) throw std::runtime_error("source ASDL must not import libraries");
        asdl::resolve_options(result);
        asdl::validate(result);
        return result;
    }();
    return schema;
}

void text(lua_State* lua, const char* field, std::string_view value) {
    lua_pushlstring(lua, value.empty() ? "" : value.data(), value.size());
    lua_setfield(lua, -2, field);
}

void optional_text(lua_State* lua, const char* field, std::string_view value) {
    if (!value.empty()) text(lua, field, value);
}

void integer(lua_State* lua, const char* field, std::size_t value) {
    if (!std::in_range<lua_Integer>(value)) throw std::runtime_error("source value exceeds ASDL integer range");
    lua_pushinteger(lua, static_cast<lua_Integer>(value));
    lua_setfield(lua, -2, field);
}

void boolean(lua_State* lua, const char* field, bool value) {
    lua_pushboolean(lua, value);
    lua_setfield(lua, -2, field);
}

void location(lua_State* lua, const Location& where) {
    lua_newtable(lua);
    text(lua, "file", where.file);
    integer(lua, "offset", where.offset);
    integer(lua, "line", where.line);
    integer(lua, "column", where.column);
    lua_setfield(lua, -2, "where");
}

void push_import(lua_State* lua, const Import& item) {
    lua_newtable(lua);
    text(lua, "tag", item.kind == Import::Kind::Program ? "ProgramRequest" : "ModuleRequest");
    text(lua, "spelling", item.module);
    optional_text(lua, "alias", item.alias);
    integer(lua, "relative", item.relative);
    boolean(lua, "quoted", item.quoted);
    boolean(lua, "selective", item.selective);
    location(lua, item.where);
    lua_newtable(lua);
    lua_Integer i = 0;
    for (const auto& selector : item.names) {
        lua_newtable(lua);
        text(lua, "name", selector.name);
        optional_text(lua, "alias", selector.alias);
        boolean(lua, "is_operator", selector.is_operator);
        lua_rawseti(lua, -2, ++i);
    }
    lua_setfield(lua, -2, "selectors");
}

template<class Add>
void explicit_declarations(const Unit& unit, Add add) {
    const auto record = [&](const auto& d, const char* kind) {
        if (d.visibility.kind != DeclarationVisibility::Unspecified)
            add(d.where, d.name, kind, d.visibility);
    };
    for (const auto& f : unit.functions) record(f, "function");
    for (const auto& d : unit.nominals) record(d, "type");
    for (const auto& d : unit.cliches) record(d, "cliche");
    for (const auto& m : unit.macros) record(m, "macro");
    for (const auto& o : unit.operators)
        if (o.header.visibility.kind != DeclarationVisibility::Unspecified)
            add(o.header.where, o.header.lexeme, "operator", o.header.visibility);
}

void push_declaration(lua_State* lua, const Location& where, std::string_view name,
                      const char* kind, VisibilityRule visibility) {
    lua_newtable(lua);
    text(lua, "name", name);
    text(lua, "kind", kind);
    location(lua, where);
    switch (visibility.kind) {
    case DeclarationVisibility::Public: text(lua, "visibility", "SourcePublic"); break;
    case DeclarationVisibility::Private: text(lua, "visibility", "SourcePrivate"); break;
    case DeclarationVisibility::Shared:
        text(lua, "visibility", "SourceShared");
        lua_newtable(lua);
        text(lua, "elevation", std::to_string(visibility.elevation));
        lua_setfield(lua, -2, "fields");
        break;
    case DeclarationVisibility::Unspecified:
        throw std::runtime_error("unexpected unspecified declaration in explicit source visibility");
    }
}

void push_module(lua_State* lua, const Unit& unit) {
    lua_newtable(lua);
    text(lua, "kind", "module");
    if (unit.module) {
        lua_newtable(lua);
        text(lua, "name", unit.module->name);
        location(lua, unit.module->where);
        lua_setfield(lua, -2, "header");
    }
    lua_newtable(lua);
    lua_Integer index = 0;
    for (const auto& item : unit.module_imports) {
        push_import(lua, item);
        lua_rawseti(lua, -2, ++index);
    }
    lua_setfield(lua, -2, "imports");
    lua_newtable(lua);
    index = 0;
    explicit_declarations(unit, [&](const auto& where, const auto& name, auto kind, auto visibility) {
        push_declaration(lua, where, name, kind, visibility);
        lua_rawseti(lua, -2, ++index);
    });
    lua_setfield(lua, -2, "declarations");
}

void push_signature(lua_State* lua, const ModuleSignature& signature) {
    lua_newtable(lua);
    text(lua, "kind", "signature");
    text(lua, "name", signature.module.value_or(""));
    location(lua, signature.where);
    lua_newtable(lua);
    lua_Integer index = 0;
    const auto member = [&](const Location& where, const char* tag, std::string_view name,
                            std::optional<std::size_t> arity = {}) {
        lua_newtable(lua);
        text(lua, "tag", tag);
        text(lua, "name", name);
        if (arity) integer(lua, "arity", *arity);
        location(lua, where);
        lua_rawseti(lua, -2, ++index);
    };
    for (const auto& type : signature.types)
        member(type.where, "AbstractType", type.name, type.arity);
    for (const auto& item : signature.members)
        member(item.where, item.procedure ? "ProcedureSignature" : "FunctionSignature", item.name);
    lua_setfield(lua, -2, "members");
}

void push_graph(lua_State* lua, const LoadedUnit& graph, const Location& where) {
    lua_newtable(lua);
    text(lua, "kind", "graph");
    location(lua, where);
    lua_newtable(lua);
    std::map<std::filesystem::path, std::size_t> ids;
    lua_Integer index = 0;
    for (const auto& path : graph.dependencies) {
        const auto spelling = path.string();
        lua_pushlstring(lua, spelling.data(), spelling.size());
        lua_rawseti(lua, -2, ++index);
        ids.emplace(path, static_cast<std::size_t>(index));
    }
    lua_setfield(lua, -2, "paths");
    const auto id = [&](const auto& path) -> std::size_t {
        auto found = ids.find(path);
        return found == ids.end() ? 0 : found->second;
    };
    lua_newtable(lua);
    index = 0;
    for (const auto& [importer, provider] : graph.source_edges) {
        lua_newtable(lua);
        integer(lua, "importer", id(importer));
        integer(lua, "provider", id(provider));
        lua_rawseti(lua, -2, ++index);
    }
    lua_setfield(lua, -2, "edges");
}

void push_unused(lua_State* lua, std::span<const UnusedBinding> observations) {
    lua_newtable(lua);
    text(lua, "kind", "unused");
    lua_newtable(lua);
    lua_Integer index = 0;
    for (const auto& item : observations) {
        lua_newtable(lua);
        text(lua, "tag", unused_pattern(item.kind).tag);
        text(lua, "name", item.name);
        text(lua, "kind", item.binding_kind);
        location(lua, item.where);
        lua_rawseti(lua, -2, ++index);
    }
    lua_setfield(lua, -2, "bindings");
}

struct Library {
    const char* key;
    const char* name;
    std::string_view bytes;
    const char* version="1";
};
constexpr Library libraries[]{
    {"policy", "on1x.source.policy", source_asdl::policy},
    {"imports", "on1x.source.imports", source_asdl::imports},
    {"signatures", "on1x.source.signatures", source_asdl::signatures},
    {"graph", "on1x.source.graph", source_asdl::graph},
    {"unused", "on1x.source.unused", source_asdl::unused},
};
constexpr Library diagnostics_library{"diagnostics", "std.diagnostics", source_asdl::std_diagnostics, nullptr};

void install_signer(lua_State* lua) {
    // This pinned installer always reads LIBSTONE_ASDL_PATH to populate its
    // registry. Do not expose any closure that can read roots or open files.
    // The empty stdlib root also suppresses its configured install default.
    asdl_loader_install(lua, 1, "");
    lua_getfield(lua, 1, "lib");
    lua_newtable(lua);
    for (const char* name : {"new", "version", "describe"}) {
        lua_getfield(lua, -2, name);
        lua_setfield(lua, -2, name);
    }
    lua_setfield(lua, 1, "lib");
    lua_pop(lua, 1);
}

void push_library(lua_State* lua, const Library& library) {
    // Execute only compiler-embedded text, not loader path/envelope parsing.
    // Each bundle gets only the named utilities and its own LIB binding.
    lua_newtable(lua);
    const int environment = lua_gettop(lua);
    for (const char* name : {"asdl", "ipairs", "tonumber", "pairs", "next", "assert", "type", "tostring", "error", "string"}) {
        lua_getglobal(lua, name);
        lua_setfield(lua, environment, name);
    }
    const auto chunk = "=" + std::string(library.name);
    if (luaL_loadbufferx(lua, library.bytes.data(), library.bytes.size(), chunk.c_str(), "t") != LUA_OK)
        lua_error(lua);
    lua_pushvalue(lua, environment);
    if (!lua_setupvalue(lua, -2, 1)) throw std::runtime_error("missing signed ASDL library environment");
    lua_call(lua, 0, 1);
    const int exports = lua_gettop(lua);
    lua_getfield(lua, environment, "LIB");
    if (!lua_istable(lua, exports) || !lua_rawequal(lua, exports, -1))
        throw std::runtime_error("wrong signed ASDL library result");
    lua_pop(lua, 1);
    lua_getfield(lua, 1, "lib");
    lua_getfield(lua, -1, "describe");
    lua_pushvalue(lua, exports);
    lua_call(lua, 1, 1);
    lua_getfield(lua, -1, "signed");
    const bool signed_exports = lua_toboolean(lua, -1);
    lua_pop(lua, 1);
    lua_getfield(lua, -1, "name");
    const char* name = lua_tostring(lua, -1);
    const bool correct_name = name && std::string_view(name) == library.name;
    lua_pop(lua, 1);
    lua_getfield(lua, -1, "version");
    const char* version = lua_tostring(lua, -1);
    if (!signed_exports || !correct_name ||
        (library.version && (!version || std::string_view(version) != library.version)))
        throw std::runtime_error("wrong signed ASDL library identity or version");
    lua_settop(lua, exports);
    lua_remove(lua, environment);
}

struct Input {
    const Location& where;
    const Unit* unit = nullptr;
    const ModuleSignature* signature = nullptr;
    const LoadedUnit* graph = nullptr;
    std::span<const UnusedBinding> observations{};
};

// Called under LuaSession::extend's protected call and instruction budget.
int initialize(lua_State* lua) {
    const auto& input = *static_cast<const Input*>(lua_touserdata(lua, 2));
    install_signer(lua);
    asdl::push_execution(lua, module_schema());
    lua_pop(lua, 1); // private execution controller; hooks use asdl.run_logistics

    lua_newtable(lua);
    for (const auto& library : libraries) {
        push_library(lua, library);
        lua_setfield(lua, -2, library.key);
    }
    if (!input.observations.empty()) {
        push_library(lua, diagnostics_library);
        lua_setfield(lua, -2, diagnostics_library.key);
    }
    lua_setglobal(lua, "on1x_source_libraries");
    if (input.unit) push_module(lua, *input.unit);
    else if (input.signature) push_signature(lua, *input.signature);
    else if (input.graph) push_graph(lua, *input.graph, input.where);
    else push_unused(lua, input.observations);
    lua_setglobal(lua, "on1x_source_facts");
    return 0;
}

std::string read_text(lua_State* lua, const char* key) {
    lua_getfield(lua, -1, key);
    std::size_t n = 0;
    const char* value = lua_tolstring(lua, -1, &n);
    std::string result = value ? std::string(value, n) : std::string();
    lua_pop(lua, 1);
    return result;
}

std::size_t read_integer(lua_State* lua, const char* key) {
    lua_getfield(lua, -1, key);
    auto value = static_cast<std::size_t>(lua_tointeger(lua, -1));
    lua_pop(lua, 1);
    return value;
}

int read_error(lua_State* lua) {
    lua_getfield(lua, 1, "diagnostic");
    lua_getfield(lua, -1, "records");
    lua_call(lua, 0, 1);
    for (lua_Integer i = 1; i <= static_cast<lua_Integer>(lua_rawlen(lua, -1)); ++i) {
        lua_rawgeti(lua, -1, i);
        auto severity = read_text(lua, "severity");
        if (severity == "error" || severity == "fatal") {
            auto message = read_text(lua, "message");
            auto code = read_text(lua, "code");
            lua_getfield(lua, -1, "location");
            Location where{read_text(lua, "file"), read_integer(lua, "offset"),
                           read_integer(lua, "line"), read_integer(lua, "column")};
            *static_cast<std::optional<Error>*>(lua_touserdata(lua, 2)) =
                Error(std::move(where), std::move(message), code.empty() ? "ON1X-MODULE-ASDL" : std::move(code));
            return 0;
        }
        lua_pop(lua, 1);
    }
    return 0;
}

struct Warnings {
    const diagnostic::Level& level;
    std::vector<diagnostic::Record>& records;
};
int read_warnings(lua_State* lua) {
    auto& output = *static_cast<Warnings*>(lua_touserdata(lua, 2));
    lua_getfield(lua, 1, "diagnostic");
    lua_getfield(lua, -1, "records");
    lua_call(lua, 0, 1);
    for (lua_Integer i = 1; i <= static_cast<lua_Integer>(lua_rawlen(lua, -1)); ++i) {
        lua_rawgeti(lua, -1, i);
        diagnostic::Record record{.id=read_text(lua,"code"),.message=read_text(lua,"message"),
            .category=unused_group,.phase="frontend",.origin="On1x.asdl",.level=output.level,
            .name=read_text(lua,"name"),.group=read_text(lua,"group")};
        if (read_text(lua, "severity") != "warning" || record.name.empty() || record.group != unused_group)
            throw std::runtime_error("missing unused ASDL diagnostic metadata");
        lua_getfield(lua, -1, "location");
        record.where = {read_text(lua, "file"), read_integer(lua, "offset"),
                        read_integer(lua, "line"), read_integer(lua, "column")};
        output.records.push_back(std::move(record));
        lua_pop(lua, 2);
    }
    return 0;
}

void check(Input input) {
    std::optional<Error> reported;
    try {
        asdl::LuaSession session(module_schema());
        session.extend(initialize, &input);
        session.run(source_asdl::adapter, "=On1x source ASDL adapter");
        if (session.diagnostic_count("error") || session.diagnostic_count("fatal")) {
            session.extend(read_error, &reported);
            if (!reported) throw std::runtime_error("missing source ASDL diagnostic record");
        }
    } catch (const std::runtime_error& error) {
        throw Error(input.where, "source ASDL logistics failed: " + std::string(error.what()), "ON1X-MODULE-ASDL");
    }
    if (reported) throw std::move(*reported);
}

} // namespace

void check_asdl_module(const Unit& unit) {
    const Location* where = unit.module ? &unit.module->where : nullptr;
    if (!where) {
        auto first = std::ranges::find(unit.module_imports, Import::Kind::Program, &Import::kind);
        if (first != unit.module_imports.end()) where = &first->where;
    }
    if (!where) {
        explicit_declarations(unit, [&](const Location& at, auto&&, auto, VisibilityRule visibility) {
            if (!where && visibility.kind == DeclarationVisibility::Shared) where = &at;
        });
    }
    if (where) check({.where = *where, .unit = &unit});
}

void check_asdl_signature(const ModuleSignature& signature) {
    if (signature.module) check({.where = signature.where, .signature = &signature});
}

void check_asdl_graph(const LoadedUnit& graph, const Location& where) {
    check({.where = where, .graph = &graph});
}

std::vector<diagnostic::Record> collect_asdl_unused(
        const std::vector<UnusedBinding>& observations, const diagnostic::Level& level) {
    std::vector<diagnostic::Record> records;
    Warnings output{level,records};
    auto bounded=std::span(observations).first(std::min(observations.size(),max_unused_reports));
    while (!bounded.empty()) {
        auto batch=bounded.first(std::min(bounded.size(),std::size_t{128}));
        bounded=bounded.subspan(batch.size());
        Input input{.where=batch.front().where,.observations=batch};
        const auto before=records.size();
        try {
            asdl::LuaSession session(module_schema());
            session.extend(initialize, &input);
            session.run(source_asdl::adapter, "=On1x source ASDL adapter");
            if (session.diagnostic_count("warning") != batch.size())
                throw std::runtime_error("source ASDL did not report every unused binding");
            session.extend(read_warnings, &output);
        } catch (const std::runtime_error&) {
            records.resize(before);
            native_unused_records(batch,level,records);
        }
    }
    return records;
}

} // namespace on1x
#else
void on1x::check_asdl_module(const Unit&) {}
void on1x::check_asdl_signature(const ModuleSignature&) {}
void on1x::check_asdl_graph(const LoadedUnit&, const Location&) {}
std::vector<on1x::diagnostic::Record> on1x::collect_asdl_unused(
        const std::vector<UnusedBinding>& observations, const diagnostic::Level& level) {
    std::vector<diagnostic::Record> records;
    native_unused_records(std::span(observations).first(std::min(observations.size(),max_unused_reports)),level,records);
    return records;
}
#endif
