#include "diagnostic_filter.hpp"

#ifdef ON1X_ASDL_DIAGNOSTIC_SELECTORS
#include <asdl/lua.hpp>
#include <asdl/schema.hpp>
#include "asdl_loader.h"
#include "on1x_std_diagnostics.hpp"

extern "C" {
#include <lua.h>
}
#endif

namespace on1x::diagnostic {
namespace {

bool valid_label(std::string_view label) {
    return !label.empty() && label.size() <= 127 &&
        label.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-") ==
            std::string_view::npos;
}

#ifdef ON1X_ASDL_DIAGNOSTIC_SELECTORS
constexpr char library[] = "on1x_presentation_diagnostics";

void text(lua_State* lua, std::string_view text) {
    lua_pushlstring(lua, text.empty() ? "" : text.data(), text.size());
}

void begin(lua_State* lua, const char* symbol) {
    lua_getglobal(lua, library);
    lua_getfield(lua, -1, symbol);
}

int install(lua_State* lua) {
    // The only executed text is the build-time pinned LibStone stdlib source.
    // No user-controlled search path is used to resolve the library.
    return asdl_loader_install(lua, 1, "");
}

struct Selection {
    std::string_view selector, key, label;
    bool enabled;
};

int set_selection(lua_State* lua) {
    const auto& op = *static_cast<const Selection*>(lua_touserdata(lua, 2));
    begin(lua, op.enabled ? "Enable" : "Disable");
    if (!op.key.empty()) {
        lua_newtable(lua);
        text(lua, op.label);
        lua_setfield(lua, -2, op.key == "name" ? "name" : "group");
    } else if (op.selector == "all") {
        lua_pushnil(lua);
    } else {
        text(lua, op.selector);
    }
    lua_call(lua, 1, 0);
    return 0;
}

struct Query {
    Severity severity;
    std::string_view name, group;
    bool visible = true;
};

int query_selection(lua_State* lua) {
    auto& query = *static_cast<Query*>(lua_touserdata(lua, 2));
    begin(lua, "IsEnabled");
    lua_pushstring(lua, query.severity == Severity::Error ? "error" :
                       query.severity == Severity::Warning ? "warning" : "info");
    lua_newtable(lua);
    if (!query.name.empty()) {
        text(lua, query.name);
        lua_setfield(lua, -2, "name");
    }
    if (!query.group.empty()) {
        text(lua, query.group);
        lua_setfield(lua, -2, "group");
    }
    lua_call(lua, 2, 1);
    query.visible = lua_toboolean(lua, -1);
    return 0;
}
#endif
} // namespace

struct PresentationFilter::Impl {
#ifdef ON1X_ASDL_DIAGNOSTIC_SELECTORS
    static const asdl::Schema& schema() {
        static const auto schema = asdl::parse("module DiagnosticPresentation { marker = Marker }",
                                               "=diagnostic selectors");
        return schema;
    }
    asdl::LuaSession session{schema()};
    std::size_t count = 0;

    Impl() {
        session.extend(install);
        session.run(source_asdl::std_diagnostics, "=LibStone std.diagnostics");
        session.run("on1x_presentation_diagnostics = LIB; LIB = nil", "=diagnostic selectors");
    }
#endif
};

PresentationFilter::PresentationFilter() = default;
PresentationFilter::~PresentationFilter() = default;

void PresentationFilter::set(std::string_view selector, bool enabled) {
    const auto colon = selector.find(':');
    const auto key = colon == std::string_view::npos ? std::string_view{} : selector.substr(0, colon);
    const auto label = colon == std::string_view::npos ? std::string_view{} : selector.substr(colon + 1);
    if (selector != "all" && selector != "error" && selector != "warning" && selector != "info" &&
        !((key == "name" || key == "group") && valid_label(label)))
        throw std::runtime_error("expected diagnostic selector: all, error, warning, info, name:ID, or group:ID");
#ifdef ON1X_ASDL_DIAGNOSTIC_SELECTORS
    if (!impl_) impl_ = std::make_unique<Impl>();
    if (impl_->count >= max_selectors) throw std::runtime_error("diagnostic selector count budget exceeded");
    Selection operation{selector, key, label, enabled};
    impl_->session.extend(set_selection, &operation);
    ++impl_->count;
#else
    (void)enabled;
    throw std::runtime_error("diagnostic selectors require the LibStone ASDL frontend");
#endif
}

bool PresentationFilter::visible(Severity severity, std::string_view name, std::string_view group) const {
#ifdef ON1X_ASDL_DIAGNOSTIC_SELECTORS
    if (impl_) {
        Query query{severity, name, group};
        impl_->session.extend(query_selection, &query);
        return query.visible;
    }
#else
    (void)severity;
    (void)name;
    (void)group;
#endif
    return true;
}

bool PresentationFilter::visible(const Record& record) const {
    return visible(record.level.kind, record.name.empty() ? record.id : record.name,
                   record.group.empty() ? record.category : record.group);
}

} // namespace on1x::diagnostic
