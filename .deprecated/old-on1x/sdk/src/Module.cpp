#include "On1xSDK-Module.h"
#include "MetadataInternal.hpp"

#include <vector>

namespace {

using namespace on1x::sdk;
namespace md = on1x::sdk::metadata;

constexpr auto kind = Kind::Module;
using D = On1xSDK_Module;
using F = md::Field<D>;

bool valid_module(std::string_view name) noexcept {
    bool start = true;
    for (char c : name) {
        if (start) {
            if (!md::letter(c) && c != '_') return false;
            start = false;
        } else if (c == '.') {
            start = true;
        } else if (!md::alnum(c) && c != '_') {
            return false;
        }
    }
    return !start;
}

struct OwnedModule : md::Storage<D, 3> {
    std::vector<std::string> names;
    std::vector<const char*> imports;

    OwnedModule(const D& module, const std::array<F, 3>& fields,
                const std::array<std::string_view, ON1X_SDK_MAX_MODULE_IMPORTS>& views)
        : Storage(module, fields), names(module.import_count), imports(module.import_count) {
        for (size_t i = 0; i < module.import_count; ++i) {
            names[i].assign(views[i]);
            imports[i] = names[i].c_str();
        }
        descriptor.imports = imports.empty() ? nullptr : imports.data();
    }
};

} // namespace

extern "C" On1xSDK_Status on1x_sdk_module_register(On1xSDK_Registry* registry, const On1xSDK_Module* module) {
    if (const auto status = md::check(registry, module); status != ON1X_SDK_OK) return status;
    if (module->import_count > ON1X_SDK_MAX_MODULE_IMPORTS) return ON1X_SDK_LIMIT;
    if ((module->import_count == 0) != (module->imports == nullptr)) return ON1X_SDK_INVALID_ARGUMENT;

    md::Validation validate;
    const auto name = validate.field(module->name, ON1X_SDK_NAME_MAX, valid_module);
    const std::array fields{
        F{&D::name, name},
        F{&D::revision, validate.text(module->revision, ON1X_SDK_NAME_MAX)},
        F{&D::summary, validate.text(module->summary, ON1X_SDK_MAX_MODULE_SUMMARY_BYTES, true)}
    };
    if (validate.status != ON1X_SDK_OK) return validate.status;
    std::array<std::string_view, ON1X_SDK_MAX_MODULE_IMPORTS> imports;
    for (size_t i = 0; i < module->import_count; ++i) {
        const auto dep = validate.field(module->imports[i], ON1X_SDK_NAME_MAX, valid_module);
        if (validate.status != ON1X_SDK_OK) return validate.status;
        if (dep == name) return ON1X_SDK_INVALID_ARGUMENT;
        for (size_t j = 0; j < i; ++j)
            if (dep == imports[j]) return ON1X_SDK_DUPLICATE;
        imports[i] = dep;
    }
    return md::guard([&] {
        return md::publish(registry, kind, std::make_shared<OwnedModule>(*module, fields, imports));
    });
}

extern "C" const On1xSDK_Module* on1x_sdk_module_find(const On1xSDK_Registry* registry, const char* name) {
    return md::lookup<On1xSDK_Module>(registry, kind, name);
}

extern "C" size_t on1x_sdk_module_count(const On1xSDK_Registry* registry) {
    return count(registry, kind);
}

extern "C" const On1xSDK_Module* on1x_sdk_module_at(const On1xSDK_Registry* registry, size_t index) {
    return static_cast<const On1xSDK_Module*>(at(registry, kind, index));
}
