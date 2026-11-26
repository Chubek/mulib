#include <on1x/asdl_module.hpp>

#ifdef ON1X_ASDL_MODULES
#include <on1x/ast.hpp>
#include <asdl/execution.hpp>
#include <asdl/lua.hpp>
#include <asdl/schema.hpp>
#include "on1x_source_asdl.hpp"
#include <algorithm>
#include <string_view>
#include <utility>

extern "C" {
#include <lua.h>
}

namespace on1x {
namespace {

const asdl::Schema& module_schema() {
    static const auto schema = [] {
        auto result = asdl::parse(source_asdl::schema, "On1x.asdl");
        // This compiler path never resolves or executes ambient Lua/schema imports.
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
        integer(lua, "elevation", visibility.elevation);
        lua_setfield(lua, -2, "fields");
        break;
    case DeclarationVisibility::Unspecified:
        throw std::runtime_error("unexpected unspecified declaration in explicit source visibility");
    }
}

// Called under LuaSession::extend's protected call and instruction budget.
int initialize(lua_State* lua) {
    const auto& unit = *static_cast<const Unit*>(lua_touserdata(lua, 2));
    // No schema imports are admitted, so the signed-library loader is not needed.
    asdl::push_execution(lua, module_schema());
    lua_getfield(lua, -1, "run");
    lua_setfield(lua, 1, "run_logistics");
    lua_pop(lua, 1);

    lua_newtable(lua);
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
            lua_getfield(lua, -1, "location");
            Location where{read_text(lua, "file"), read_integer(lua, "offset"),
                           read_integer(lua, "line"), read_integer(lua, "column")};
            *static_cast<std::optional<Error>*>(lua_touserdata(lua, 2)) =
                Error(std::move(where), std::move(message), "ON1X-MODULE-ASDL");
            return 0;
        }
        lua_pop(lua, 1);
    }
    return 0;
}

} // namespace

void check_asdl_module(const Unit& unit) {
    const Location* where = unit.module ? &unit.module->where : nullptr;
    if (!where) {
        auto first = std::ranges::find(unit.module_imports, Import::Kind::Program, &Import::kind);
        if (first != unit.module_imports.end()) where = &first->where;
    }
    if (!where) return;

    std::optional<Error> reported;
    try {
        asdl::LuaSession session(module_schema());
        session.extend(initialize, const_cast<Unit*>(&unit));
        session.run(source_asdl::adapter, "=On1x source ASDL adapter");
        if (session.diagnostic_count("error") || session.diagnostic_count("fatal")) {
            session.extend(read_error, &reported);
            if (!reported) throw std::runtime_error("missing source ASDL diagnostic record");
        }
    } catch (const std::runtime_error& error) {
        throw Error(*where, "source ASDL logistics failed: " + std::string(error.what()), "ON1X-MODULE-ASDL");
    }
    if (reported) throw std::move(*reported);
}

} // namespace on1x
#else
void on1x::check_asdl_module(const Unit&) {}
#endif
