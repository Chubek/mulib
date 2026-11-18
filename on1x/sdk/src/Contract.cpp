#include "On1xSDK-Contract.h"
#include "MetadataInternal.hpp"

namespace {

using namespace on1x::sdk;
namespace md = on1x::sdk::metadata;

constexpr auto kind = Kind::Contract;
constexpr uint32_t effect_mask =
    ON1X_SDK_CONTRACT_EFFECT_UNKNOWN | ON1X_SDK_CONTRACT_EFFECT_ALLOCATE |
    ON1X_SDK_CONTRACT_EFFECT_TRAP | ON1X_SDK_CONTRACT_EFFECT_BLOCK |
    ON1X_SDK_CONTRACT_EFFECT_IO | ON1X_SDK_CONTRACT_EFFECT_MUTATE |
    ON1X_SDK_CONTRACT_EFFECT_CALLBACK | ON1X_SDK_CONTRACT_EFFECT_NONDETERMINISTIC;

bool valid_sha256(std::string_view text) noexcept {
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char c) {
        return md::digit(c) || (c >= 'a' && c <= 'f');
    });
}

} // namespace

extern "C" On1xSDK_Status on1x_sdk_contract_register(On1xSDK_Registry* registry,
                                                    const On1xSDK_Contract* contract) {
    if (const auto status = md::check(registry, contract); status != ON1X_SDK_OK) return status;
    const auto effects = contract->effects;
    if ((effects & ~effect_mask) ||
        ((effects & ON1X_SDK_CONTRACT_EFFECT_UNKNOWN) && effects != ON1X_SDK_CONTRACT_EFFECT_UNKNOWN) ||
        bool(contract->target) != bool(contract->abi_description)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    using D = On1xSDK_Contract;
    using F = md::Field<D>;
    md::Validation validate;
    constexpr auto max = ON1X_SDK_MAX_CONTRACT_TEXT_BYTES;
    const std::array fields{
        F{&D::name, validate.name(contract->name)},
        F{&D::signature, validate.text(contract->signature, max)},
        F{&D::ownership, validate.text(contract->ownership, max, true)},
        F{&D::borrowing, validate.text(contract->borrowing, max, true)},
        F{&D::target, validate.field(contract->target, ON1X_SDK_NAME_MAX, md::valid_target, true)},
        F{&D::abi_description, validate.text(contract->abi_description, max, true)},
        F{&D::failure, validate.text(contract->failure, max, true)},
        F{&D::evidence_sha256, validate.field(contract->evidence_sha256, 64, valid_sha256, true)}
    };
    if (validate.status != ON1X_SDK_OK) return validate.status;
    return md::register_fields(registry, kind, *contract, fields);
}

extern "C" const On1xSDK_Contract* on1x_sdk_contract_find(const On1xSDK_Registry* registry,
                                                         const char* name) {
    return md::lookup<On1xSDK_Contract>(registry, kind, name);
}

extern "C" size_t on1x_sdk_contract_count(const On1xSDK_Registry* registry) {
    return count(registry, kind);
}

extern "C" const On1xSDK_Contract* on1x_sdk_contract_at(const On1xSDK_Registry* registry, size_t index) {
    return static_cast<const On1xSDK_Contract*>(at(registry, kind, index));
}
