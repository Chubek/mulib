#include "On1xSDK-Diagnostic.h"
#include "On1xSDK-Module.h"
#include "On1xSDK-Registry.h"
#include "../src/RegistryInternal.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <type_traits>

namespace sdk = on1x::sdk;
using sdk::Kind;

#define CHECK(expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
        std::abort(); \
    } \
} while (false)

static_assert(ON1X_SDK_ABI_VERSION == 1);
static_assert(ON1X_SDK_OK == 0 && ON1X_SDK_ERROR == 1 &&
              ON1X_SDK_UNSUPPORTED == 2 && ON1X_SDK_LIMIT == 3);
static_assert(std::is_standard_layout_v<On1xSDK_Extension> &&
              std::is_trivial_v<On1xSDK_Extension>);
static_assert(std::is_standard_layout_v<On1xSDK_Diagnostic> &&
              std::is_trivial_v<On1xSDK_Diagnostic>);

namespace {

using RegistryPtr = std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>;

RegistryPtr make_registry() {
    On1xSDK_Registry* registry = nullptr;
    CHECK(on1x_sdk_registry_create(&registry) == ON1X_SDK_OK);
    CHECK(registry != nullptr);
    return {registry, on1x_sdk_registry_destroy};
}

On1xSDK_Extension descriptor(const char* name, On1xSDK_RegisterFn callback,
                             const char* version = "1.0+test") {
    return {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Extension), name, version, callback};
}

size_t noop_calls = 0;

extern "C" On1xSDK_Status noop(const On1xSDK_Host* host, On1xSDK_Registry* registry) {
    CHECK(host && registry);
    ++noop_calls;
    return ON1X_SDK_OK;
}

void test_validation() {
    CHECK(on1x_sdk_registry_create(nullptr) == ON1X_SDK_INVALID_ARGUMENT);
    on1x_sdk_registry_destroy(nullptr);

    const auto registry = make_registry();
    const auto good = descriptor("test.validation", noop);
    CHECK(on1x_sdk_registry_register(nullptr, &good) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_registry_register(registry.get(), nullptr) == ON1X_SDK_INVALID_ARGUMENT);

    auto bad = good;
    bad.abi_version = ON1X_SDK_ABI_VERSION + 1;
    CHECK(on1x_sdk_registry_register(registry.get(), &bad) == ON1X_SDK_ABI_MISMATCH);
    // This input really is only the ABI header. No callback field can be read.
    alignas(On1xSDK_Extension) const uint32_t short_header[]{ON1X_SDK_ABI_VERSION, 8};
    CHECK(on1x_sdk_registry_register(registry.get(),
          reinterpret_cast<const On1xSDK_Extension*>(short_header)) == ON1X_SDK_ABI_MISMATCH);
    const char* const bad_names[]{nullptr, "", "1bad", "-bad", "bad/name", "bad name", "x\xFF"};
    for (const char* name : bad_names) {
        bad = good;
        bad.name = name;
        CHECK(on1x_sdk_registry_register(registry.get(), &bad) == ON1X_SDK_INVALID_ARGUMENT);
    }
    std::array<char, ON1X_SDK_NAME_MAX + 1> unterminated;
    unterminated.fill('x');
    bad = good;
    bad.name = unterminated.data();
    CHECK(on1x_sdk_registry_register(registry.get(), &bad) == ON1X_SDK_LIMIT);
    const char* const bad_versions[]{nullptr, "", "\n", "\xFF"};
    for (const char* version : bad_versions) {
        bad = good;
        bad.version = version;
        CHECK(on1x_sdk_registry_register(registry.get(), &bad) == ON1X_SDK_INVALID_ARGUMENT);
    }
    bad = good;
    bad.version = unterminated.data();
    CHECK(on1x_sdk_registry_register(registry.get(), &bad) == ON1X_SDK_LIMIT);
    bad = good;
    bad.register_all = nullptr;
    CHECK(on1x_sdk_registry_register(registry.get(), &bad) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(noop_calls == 0);

    struct LargerDescriptor {
        On1xSDK_Extension base;
        uint64_t ignored;
    } larger{good, 0};
    larger.base.struct_size = sizeof(larger);
    CHECK(on1x_sdk_registry_register(registry.get(), &larger.base) == ON1X_SDK_OK);
    CHECK(on1x_sdk_registry_register(registry.get(), &good) == ON1X_SDK_DUPLICATE);
    CHECK(noop_calls == 1);

    auto name = std::string(ON1X_SDK_NAME_MAX, 'x');
    auto version = std::string(ON1X_SDK_NAME_MAX, 'v');
    const auto original_name = name;
    const auto max_length = descriptor(name.c_str(), noop, version.c_str());
    CHECK(on1x_sdk_registry_register(registry.get(), &max_length) == ON1X_SDK_OK);
    name.assign("shorter_name");
    version.assign("different_version");
    const auto duplicate = descriptor(original_name.c_str(), noop, "2.0");
    CHECK(on1x_sdk_registry_register(registry.get(), &duplicate) == ON1X_SDK_DUPLICATE);
    CHECK(noop_calls == 2);
}

std::weak_ptr<const int> pending;
On1xSDK_Status result;
const void* preexisting_address;

extern "C" On1xSDK_Status register_entries(const On1xSDK_Host* host, On1xSDK_Registry* registry) {
    CHECK(host && registry);
    CHECK(sdk::count(registry, Kind::Analyzer) == 1);
    CHECK(sdk::at(registry, Kind::Analyzer, 0) == preexisting_address);
    CHECK(!sdk::find(registry, Kind::Analyzer, "extension.entry"));

    const auto nested = descriptor("independent-name", noop);
    CHECK(on1x_sdk_registry_register(registry, &nested) == ON1X_SDK_BUSY);
    on1x_sdk_registry_destroy(registry); // Must not invalidate an active callback.

    const auto entry = std::make_shared<const int>(42);
    pending = entry;
    CHECK(sdk::add(registry, Kind::Analyzer, "extension.entry", entry) == ON1X_SDK_OK);
    char module_name[] = "extension.entry";
    const On1xSDK_Module module{ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Module),
                               module_name, "1", nullptr, 0, nullptr};
    CHECK(on1x_sdk_module_register(registry, &module) == ON1X_SDK_OK);
    module_name[0] = '_';

    CHECK(sdk::find(registry, Kind::Analyzer, "extension.entry") == entry.get());
    CHECK(sdk::at(registry, Kind::Analyzer, 0) == preexisting_address);
    CHECK(sdk::at(registry, Kind::Analyzer, 1) == entry.get());
    CHECK(sdk::count(registry, Kind::Analyzer) == 2);
    CHECK(on1x_sdk_module_count(registry) == 1);
    const auto* stored = on1x_sdk_module_find(registry, "extension.entry");
    CHECK(stored != nullptr);
    CHECK(stored == on1x_sdk_module_at(registry, 0));
    CHECK(std::strcmp(stored->name, "extension.entry") == 0);
    CHECK(!on1x_sdk_module_find(registry, module_name));

    CHECK(sdk::add(registry, Kind::Analyzer, "extension.entry", entry) == ON1X_SDK_DUPLICATE);
    CHECK(sdk::add(registry, Kind::Analyzer, "preexisting", entry) == ON1X_SDK_DUPLICATE);
    return result;
}

extern "C" On1xSDK_Status throw_after_add(const On1xSDK_Host* host, On1xSDK_Registry* registry) {
    CHECK(register_entries(host, registry) == ON1X_SDK_OK);
    throw 42;
}

extern "C" On1xSDK_Status fail_allocation(const On1xSDK_Host* host, On1xSDK_Registry* registry) {
    CHECK(register_entries(host, registry) == ON1X_SDK_OK);
    throw std::bad_alloc{};
}

void test_transactions() {
    auto registry = make_registry();
    auto name = std::string("preexisting");
    const auto descriptor_data = std::make_shared<const int>(9);
    CHECK(sdk::add(registry.get(), Kind::Analyzer, name, descriptor_data) == ON1X_SDK_OK);
    name.assign("mutated");
    preexisting_address = descriptor_data.get();
    CHECK(sdk::find(registry.get(), Kind::Analyzer, "preexisting") == preexisting_address);
    CHECK(sdk::find(registry.get(), Kind::Analyzer, name) == nullptr);

    const auto check_rolled_back = [&] {
        CHECK(pending.expired());
        CHECK(sdk::count(registry.get(), Kind::Analyzer) == 1);
        CHECK(on1x_sdk_module_count(registry.get()) == 0);
        CHECK(sdk::at(registry.get(), Kind::Analyzer, 0) == preexisting_address);
        CHECK(sdk::at(registry.get(), Kind::Analyzer, 1) == nullptr);
        CHECK(!sdk::find(registry.get(), Kind::Analyzer, "extension.entry"));
        CHECK(!on1x_sdk_module_find(registry.get(), "extension.entry"));
    };
    const auto extension = descriptor("transaction", register_entries);
    for (const auto failure : {ON1X_SDK_UNSUPPORTED, ON1X_SDK_DUPLICATE, ON1X_SDK_LIMIT}) {
        result = failure;
        CHECK(on1x_sdk_registry_register(registry.get(), &extension) == failure);
        check_rolled_back();
    }
    result = static_cast<On1xSDK_Status>(200);
    CHECK(on1x_sdk_registry_register(registry.get(), &extension) == ON1X_SDK_ERROR);
    check_rolled_back();

    result = ON1X_SDK_OK;
    const auto throwing_extension = descriptor("transaction", throw_after_add);
    CHECK(on1x_sdk_registry_register(registry.get(), &throwing_extension) == ON1X_SDK_ERROR);
    check_rolled_back();
    const auto allocation_failure = descriptor("transaction", fail_allocation);
    CHECK(on1x_sdk_registry_register(registry.get(), &allocation_failure) == ON1X_SDK_LIMIT);
    check_rolled_back();
    CHECK(on1x_sdk_registry_register(registry.get(), &extension) == ON1X_SDK_OK);
    CHECK(!pending.expired());
    CHECK(sdk::count(registry.get(), Kind::Analyzer) == 2);
    CHECK(on1x_sdk_module_count(registry.get()) == 1);
    const auto* committed_module = on1x_sdk_module_find(registry.get(), "extension.entry");
    CHECK(committed_module && std::strcmp(committed_module->name, "extension.entry") == 0);
    const auto* committed = sdk::find(registry.get(), Kind::Analyzer, "extension.entry");
    CHECK(committed && *static_cast<const int*>(committed) == 42);

    const auto nested = descriptor("independent-name", noop);
    CHECK(on1x_sdk_registry_register(registry.get(), &nested) == ON1X_SDK_OK);
    CHECK(on1x_sdk_registry_register(registry.get(), &extension) == ON1X_SDK_DUPLICATE);
    CHECK(sdk::at(registry.get(), Kind::Analyzer, 1) == committed);
    registry.reset();
    CHECK(pending.expired());
}

void test_lookup_and_limits() {
    const auto registry = make_registry();
    const auto entry = std::make_shared<const int>(1);
    constexpr auto invalid_kind = static_cast<Kind>(-1);
    CHECK(sdk::add(nullptr, Kind::Analyzer, "x", entry) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(sdk::add(registry.get(), invalid_kind, "x", entry) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(sdk::add(registry.get(), Kind::Analyzer, "x", {}) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(!sdk::valid_name(std::string_view("bad\0suffix", 10)));
    CHECK(!sdk::valid_name(std::string(ON1X_SDK_NAME_MAX + 1, 'a')));
    CHECK(sdk::valid_name("_good-name.42"));
    CHECK(!sdk::valid_name("bad\na"));
    CHECK(!sdk::find(nullptr, Kind::Analyzer, "x"));
    CHECK(!sdk::find(registry.get(), invalid_kind, "x"));
    CHECK(!sdk::at(nullptr, Kind::Analyzer, 0));
    CHECK(!sdk::at(registry.get(), invalid_kind, 0));
    CHECK(!sdk::at(registry.get(), Kind::Analyzer, 0));
    CHECK(sdk::count(nullptr, Kind::Analyzer) == 0);
    CHECK(sdk::count(registry.get(), invalid_kind) == 0);

    for (size_t i = 0; i < 1024; ++i) {
        const auto name = "item-" + std::to_string(i);
        CHECK(sdk::add(registry.get(), Kind::Toolchain, name, entry) == ON1X_SDK_OK);
        const auto extension = descriptor(name.c_str(), noop);
        CHECK(on1x_sdk_registry_register(registry.get(), &extension) == ON1X_SDK_OK);
    }
    CHECK(sdk::count(registry.get(), Kind::Toolchain) == 1024);
    CHECK(sdk::at(registry.get(), Kind::Toolchain, 1023) == entry.get());
    CHECK(!sdk::at(registry.get(), Kind::Toolchain, SIZE_MAX));
    CHECK(sdk::add(registry.get(), Kind::Toolchain, "extra", entry) == ON1X_SDK_LIMIT);
    CHECK(sdk::add(registry.get(), Kind::Toolchain, "item-0", entry) == ON1X_SDK_DUPLICATE);
    CHECK(sdk::add(registry.get(), Kind::ExecEnv, "extra", entry) == ON1X_SDK_OK);
    const auto rejected = descriptor("extra", nullptr);
    CHECK(on1x_sdk_registry_register(registry.get(), &rejected) == ON1X_SDK_INVALID_ARGUMENT);
    const auto excess = descriptor("extra", noop);
    const auto calls_before_excess = noop_calls;
    CHECK(on1x_sdk_registry_register(registry.get(), &excess) == ON1X_SDK_LIMIT);
    CHECK(noop_calls == calls_before_excess);
}

} // namespace

int main() {
    test_validation();
    test_transactions();
    test_lookup_and_limits();
}
