#include "On1xSDK-CLIOpt.h"
#include "On1xSDK-Contract.h"
#include "On1xSDK-ExecEnv.h"
#include "On1xSDK-Module.h"
#include "On1xSDK-Registry.h"
#include "On1xSDK-Toolchain.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>

namespace {

#define CHECK(expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
        std::abort(); \
    } \
} while (false)

using namespace std::string_view_literals;

void contracts(On1xSDK_Registry* registry) {
    char name[] = "demo.foreign";
    char signature[] = "i64 -> i64";
    char evidence[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    On1xSDK_Contract contract{
        ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Contract), name, signature, "consumes argument 0",
        "read-only during call", ON1X_SDK_CONTRACT_EFFECT_UNKNOWN,
        "not_a_real-arch", "unverified calling convention", "checked status", evidence
    };
    CHECK(on1x_sdk_contract_register(registry, &contract) == ON1X_SDK_OK);
    CHECK(on1x_sdk_contract_register(registry, &contract) == ON1X_SDK_DUPLICATE);
    name[0] = 'x';
    signature[0] = 'x';
    evidence[0] = 'b';
    const auto* view = on1x_sdk_contract_find(registry, "demo.foreign");
    CHECK(view && view != &contract);
    CHECK(view->signature == "i64 -> i64"sv && view->evidence_sha256[0] == 'a');
    CHECK(view->ownership == "consumes argument 0"sv && view->borrowing == "read-only during call"sv);
    CHECK(view->effects == ON1X_SDK_CONTRACT_EFFECT_UNKNOWN);
    CHECK(view->target == "not_a_real-arch"sv && view->abi_description == "unverified calling convention"sv);
    CHECK(view->failure == "checked status"sv);
    CHECK(on1x_sdk_contract_at(registry, 0) == view && !on1x_sdk_contract_at(registry, 1));
    CHECK(!on1x_sdk_contract_find(registry, name));

    contract.name = "invalid.contract";
    contract.effects = UINT32_MAX;
    CHECK(on1x_sdk_contract_register(registry, &contract) == ON1X_SDK_INVALID_ARGUMENT);
    contract.effects = ON1X_SDK_CONTRACT_EFFECT_UNKNOWN | ON1X_SDK_CONTRACT_EFFECT_IO;
    CHECK(on1x_sdk_contract_register(registry, &contract) == ON1X_SDK_INVALID_ARGUMENT);
    contract.effects = ON1X_SDK_CONTRACT_EFFECT_TRAP | ON1X_SDK_CONTRACT_EFFECT_IO;
    contract.target = nullptr;
    CHECK(on1x_sdk_contract_register(registry, &contract) == ON1X_SDK_INVALID_ARGUMENT);
    contract.abi_description = nullptr;
    contract.evidence_sha256 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    CHECK(on1x_sdk_contract_register(registry, &contract) == ON1X_SDK_INVALID_ARGUMENT);
    contract.evidence_sha256 = nullptr;
    contract.signature = "i64 -> \ni64";
    CHECK(on1x_sdk_contract_register(registry, &contract) == ON1X_SDK_INVALID_ARGUMENT);
    const std::string too_long(ON1X_SDK_MAX_CONTRACT_TEXT_BYTES + 1, 'x');
    contract.signature = too_long.c_str();
    CHECK(on1x_sdk_contract_register(registry, &contract) == ON1X_SDK_LIMIT);
    CHECK(on1x_sdk_contract_count(registry) == 1);
}

void modules(On1xSDK_Registry* registry) {
    char revision[] = "r1";
    char dep[] = "demo.base";
    const char* imports[] = {dep, "not.registered"};
    // Registry accepts a known ABI prefix with extra bytes but does not expose them.
    struct Extended { On1xSDK_Module module; uint32_t trailing; };
    Extended desc{{ON1X_SDK_ABI_VERSION, sizeof(Extended), "demo.module", revision, nullptr, 2, imports}, 123};
    CHECK(on1x_sdk_module_register(registry, &desc.module) == ON1X_SDK_OK);
    revision[0] = 'x';
    dep[0] = 'x';
    imports[1] = "substituted";
    const auto* view = on1x_sdk_module_find(registry, "demo.module");
    CHECK(view && view->struct_size == sizeof(On1xSDK_Module) && view->revision == "r1"sv);
    CHECK(view->import_count == 2 && view->imports != imports);
    CHECK(view->imports[0] == "demo.base"sv && view->imports[1] == "not.registered"sv);
    CHECK(!on1x_sdk_module_find(registry, "not.registered"));
    CHECK(on1x_sdk_module_at(registry, 0) == view && !on1x_sdk_module_at(registry, 1));

    auto invalid = desc.module;
    invalid.name = "demo..malformed";
    CHECK(on1x_sdk_module_register(registry, &invalid) == ON1X_SDK_INVALID_ARGUMENT);
    invalid.name = "demo.bad";
    imports[0] = "../danger";
    CHECK(on1x_sdk_module_register(registry, &invalid) == ON1X_SDK_INVALID_ARGUMENT);
    imports[0] = invalid.name;
    CHECK(on1x_sdk_module_register(registry, &invalid) == ON1X_SDK_INVALID_ARGUMENT);
    imports[0] = imports[1];
    CHECK(on1x_sdk_module_register(registry, &invalid) == ON1X_SDK_DUPLICATE);
    invalid.imports = nullptr;
    CHECK(on1x_sdk_module_register(registry, &invalid) == ON1X_SDK_INVALID_ARGUMENT);
    invalid.import_count = ON1X_SDK_MAX_MODULE_IMPORTS + 1;
    CHECK(on1x_sdk_module_register(registry, &invalid) == ON1X_SDK_LIMIT);
    CHECK(on1x_sdk_module_count(registry) == 1);
}

void cli(On1xSDK_Registry* registry, const std::string& literal) {
    char help[] = "log a label";
    On1xSDK_CLIOpt options[] = {
        {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_CLIOpt), "help", ON1X_SDK_CLIOPT_FLAG, "show help"},
        {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_CLIOpt), "sample-offset", ON1X_SDK_CLIOPT_I64, "sample offset"},
        {ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_CLIOpt), "label", ON1X_SDK_CLIOPT_STRING, help}
    };
    for (const auto& option : options) {
        CHECK(on1x_sdk_cliopt_register(registry, &option) == ON1X_SDK_OK);
    }
    help[0] = 'x';
    const auto* label = on1x_sdk_cliopt_find(registry, "label");
    CHECK(label && label->help == "log a label"sv && on1x_sdk_cliopt_at(registry, 2) == label);

    On1xSDK_CLIOptValue result{};
    CHECK(on1x_sdk_cliopt_parse(registry, "--help", &result) == ON1X_SDK_OK);
    CHECK(result.option == on1x_sdk_cliopt_at(registry, 0) && result.integer == 1 && !result.text);
    CHECK(on1x_sdk_cliopt_parse(registry, "--sample-offset=-9223372036854775808", &result) == ON1X_SDK_OK);
    CHECK(result.integer == std::numeric_limits<int64_t>::min() && !result.text);
    CHECK(on1x_sdk_cliopt_parse(registry, "--sample-offset=9223372036854775807", &result) == ON1X_SDK_OK);
    CHECK(result.integer == std::numeric_limits<int64_t>::max());
    CHECK(on1x_sdk_cliopt_parse(registry, "--label=", &result) == ON1X_SDK_OK);
    CHECK(result.option == label && result.text_size == 0 && result.text && *result.text == '\0');
    const auto arg = "--label=" + literal + "=tail";
    CHECK(on1x_sdk_cliopt_parse(registry, arg.c_str(), &result) == ON1X_SDK_OK);
    CHECK(result.text == arg.c_str() + 8 && std::string_view(result.text, result.text_size) == literal + "=tail");
    CHECK(result.integer == 0);

    for (const char* invalid : {"--", "-label=x", "--1opt", "--no--name", "--help=",
                                "--sample-offset", "--sample-offset=", "--sample-offset=+4",
                                "--sample-offset= 1", "--sample-offset=1x", "--sample-offset=0x4",
                                "--sample-offset=9223372036854775808", "--sample-offset=-9223372036854775809",
                                "--label", "--label=bad\n"}) {
        result = {label, 123, "stale", 5};
        CHECK(on1x_sdk_cliopt_parse(registry, invalid, &result) == ON1X_SDK_INVALID_ARGUMENT);
        CHECK(!result.option && !result.text && result.integer == 0 && result.text_size == 0);
    }
    CHECK(on1x_sdk_cliopt_parse(registry, "--unknown", &result) == ON1X_SDK_UNSUPPORTED);
    const auto max_value = "--label=" + std::string(ON1X_SDK_MAX_CLIOPT_VALUE_BYTES, 'x');
    CHECK(on1x_sdk_cliopt_parse(registry, max_value.c_str(), &result) == ON1X_SDK_OK);
    CHECK(result.text_size == ON1X_SDK_MAX_CLIOPT_VALUE_BYTES);
    const auto too_long_value = max_value + 'x';
    CHECK(on1x_sdk_cliopt_parse(registry, too_long_value.c_str(), &result) == ON1X_SDK_LIMIT);
    const auto too_long_name = "--" + std::string(ON1X_SDK_NAME_MAX + 1, 'x');
    CHECK(on1x_sdk_cliopt_parse(registry, too_long_name.c_str(), &result) == ON1X_SDK_LIMIT);
    CHECK(on1x_sdk_cliopt_parse(nullptr, "--help", &result) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_cliopt_parse(registry, nullptr, &result) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_cliopt_parse(registry, "--help", nullptr) == ON1X_SDK_INVALID_ARGUMENT);

    auto invalid = options[0];
    invalid.name = "--builtin";
    CHECK(on1x_sdk_cliopt_register(registry, &invalid) == ON1X_SDK_INVALID_ARGUMENT);
    invalid.name = "bad";
    invalid.kind = 0;
    CHECK(on1x_sdk_cliopt_register(registry, &invalid) == ON1X_SDK_INVALID_ARGUMENT);
    invalid.kind = ON1X_SDK_CLIOPT_FLAG;
    invalid.help = nullptr;
    CHECK(on1x_sdk_cliopt_register(registry, &invalid) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_cliopt_count(registry) == 3);
}

void toolchains_and_environments(On1xSDK_Registry* registry, const std::string& literal) {
    char driver[] = "on1x-absent-test-driver";
    // An unavailable tool and fictional target are metadata, not things to discover.
    On1xSDK_Toolchain toolchain{
        ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Toolchain), "demo", "unreal-unknown-none", driver, "r1"
    };
    CHECK(on1x_sdk_toolchain_register(registry, &toolchain) == ON1X_SDK_OK);
    driver[0] = 'x';
    const auto* tool = on1x_sdk_toolchain_find(registry, "demo");
    CHECK(tool && tool->driver_id == "on1x-absent-test-driver"sv && tool->target == "unreal-unknown-none"sv);
    CHECK(on1x_sdk_toolchain_at(registry, 0) == tool);

    toolchain.name = "bad";
    toolchain.driver_id = "/bin/sh";
    CHECK(on1x_sdk_toolchain_register(registry, &toolchain) == ON1X_SDK_INVALID_ARGUMENT);
    toolchain.driver_id = "cc; echo invalid";
    CHECK(on1x_sdk_toolchain_register(registry, &toolchain) == ON1X_SDK_INVALID_ARGUMENT);
    toolchain.driver_id = "still-logical";
    toolchain.target = "unreal--empty-component";
    CHECK(on1x_sdk_toolchain_register(registry, &toolchain) == ON1X_SDK_INVALID_ARGUMENT);
    toolchain.target = "unreal-linux";
    toolchain.abi_version += 1;
    CHECK(on1x_sdk_toolchain_register(registry, &toolchain) == ON1X_SDK_ABI_MISMATCH);
    toolchain.abi_version = ON1X_SDK_ABI_VERSION;
    toolchain.struct_size = 8;
    CHECK(on1x_sdk_toolchain_register(registry, &toolchain) == ON1X_SDK_ABI_MISMATCH);
    alignas(On1xSDK_Toolchain) const uint32_t only_header[2] = {
        ON1X_SDK_ABI_VERSION, sizeof(only_header)
    };
    CHECK(on1x_sdk_toolchain_register(registry,
        reinterpret_cast<const On1xSDK_Toolchain*>(only_header)) == ON1X_SDK_ABI_MISMATCH);
    CHECK(on1x_sdk_toolchain_count(registry) == 1);

    char toolchain_name[] = "unregistered";
    auto description = literal;
    On1xSDK_ExecEnv env{ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_ExecEnv), "demo", toolchain_name,
                      ON1X_SDK_EXECENV_SUBPROCESS | ON1X_SDK_EXECENV_NETWORK, 100, description.c_str()};
    CHECK(on1x_sdk_execenv_register(registry, &env) == ON1X_SDK_OK);
    toolchain_name[0] = 'x';
    description[0] = 'x';
    const auto* view = on1x_sdk_execenv_find(registry, "demo");
    CHECK(view && view->toolchain == "unregistered"sv && view->description == literal);
    CHECK(!on1x_sdk_toolchain_find(registry, view->toolchain));
    CHECK(view->requested_capabilities == env.requested_capabilities && view->requested_timeout_ms == 100);
    CHECK(on1x_sdk_execenv_at(registry, 0) == view && !on1x_sdk_execenv_at(registry, 1));
    CHECK(on1x_sdk_toolchain_find(registry, "demo") == tool); // Names are scoped by kind.
    env.name = "bad";
    env.requested_capabilities = UINT32_MAX;
    CHECK(on1x_sdk_execenv_register(registry, &env) == ON1X_SDK_INVALID_ARGUMENT);
    env.requested_capabilities = 0;
    env.toolchain = "../driver";
    CHECK(on1x_sdk_execenv_register(registry, &env) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_execenv_count(registry) == 1);
}

extern "C" On1xSDK_Status failing_registration(const On1xSDK_Host*, On1xSDK_Registry* registry) {
    const On1xSDK_ExecEnv env{ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_ExecEnv), "pending", nullptr, 0, 0, nullptr};
    auto status = on1x_sdk_execenv_register(registry, &env);
    if (status != ON1X_SDK_OK) return status;
    CHECK(on1x_sdk_execenv_find(registry, "pending"));
    const On1xSDK_Module invalid{
        ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Module), "pending", "v1", nullptr, 1, nullptr
    };
    return on1x_sdk_module_register(registry, &invalid);
}

} // namespace

int main() {
    On1xSDK_Registry* registry = nullptr;
    CHECK(on1x_sdk_registry_create(&registry) == ON1X_SDK_OK);

    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path marker = "on1x-sdk-no-exec-" + suffix;
    const auto literal = "$(touch '" + marker.string() + "')";
    CHECK(!std::filesystem::exists(marker));
    contracts(registry);
    modules(registry);
    cli(registry, literal);
    toolchains_and_environments(registry, literal);
    CHECK(!std::filesystem::exists(marker)); // CLI and metadata text were never interpreted.

    const On1xSDK_Extension failed{
        ON1X_SDK_ABI_VERSION, sizeof(On1xSDK_Extension), "reject.partial", "v1", failing_registration
    };
    CHECK(on1x_sdk_registry_register(registry, &failed) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(!on1x_sdk_execenv_find(registry, "pending"));
    CHECK(on1x_sdk_execenv_count(registry) == 1 && on1x_sdk_module_count(registry) == 1);

    on1x_sdk_registry_destroy(registry);
}
