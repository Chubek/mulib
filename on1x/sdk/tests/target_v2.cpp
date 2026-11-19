#include "On1xSDK-AOT.h"
#include "On1xSDK-JIT.h"
#include "On1xSDK-MachineIR.h"
#include "On1xSDK-Target.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {

using Target = std::unique_ptr<On1xSDK_Target, decltype(&on1x_sdk_target_destroy)>;

On1xSDK_DataLayout x64_layout() {
    return {ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_DataLayout),
            ON1X_SDK_BYTE_ORDER_LITTLE, 64, 64, 64, 128};
}

On1xSDK_TargetDescriptor descriptor(const char *triple, size_t triple_size,
                                    const On1xSDK_TargetFeature *features = nullptr,
                                    size_t feature_count = 0) {
    return {ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_TargetDescriptor),
            triple, triple_size, nullptr, 0, x64_layout(),
            ON1X_SDK_OBJECT_FORMAT_ELF,
            ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64,
            features, feature_count};
}

Target create(const On1xSDK_TargetDescriptor &input) {
    On1xSDK_Target *raw = nullptr;
    CHECK(on1x_sdk_target_create(&input, &raw) == ON1X_SDK_OK);
    CHECK(raw);
    return {raw, on1x_sdk_target_destroy};
}

} // namespace

int main() {
    CHECK(on1x_sdk_target_create(nullptr, nullptr) == ON1X_SDK_INVALID_ARGUMENT);
    On1xSDK_Target *raw = reinterpret_cast<On1xSDK_Target *>(1);
    CHECK(on1x_sdk_target_create(nullptr, &raw) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(raw == nullptr);
    on1x_sdk_target_destroy(nullptr);

    char triple[] = "x86_64-unknown-linux-gnu";
    char feature_a[] = "sse2";
    char feature_b[] = "avx2";
    On1xSDK_TargetFeature features[]{{feature_a, 4}, {feature_b, 4}};
    auto input = descriptor(triple, std::strlen(triple), features, 2);
    auto first = create(input);
    triple[0] = 'a';
    feature_a[0] = 'x';
    feature_b[0] = 'x';

    const On1xSDK_TargetDescriptor *view = nullptr;
    CHECK(on1x_sdk_target_view(first.get(), &view) == ON1X_SDK_OK);
    CHECK(view && std::string(view->triple, view->triple_size) ==
                      "x86_64-unknown-linux-gnu");
    CHECK(view->feature_count == 2);
    CHECK(std::string(view->features[0].data, view->features[0].size) == "avx2");
    CHECK(std::string(view->features[1].data, view->features[1].size) == "sse2");
    uint32_t present = 9;
    CHECK(on1x_sdk_target_has_feature(first.get(), "avx2", 4, &present) ==
          ON1X_SDK_OK);
    CHECK(present == 1);
    CHECK(on1x_sdk_target_has_feature(first.get(), "neon", 4, &present) ==
          ON1X_SDK_OK);
    CHECK(present == 0);
    CHECK(on1x_sdk_target_has_feature(first.get(), "bad feature", 11, &present) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_target_view(nullptr, &view) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(view == nullptr);

    const char canonical[] = "x86_64-unknown-linux-gnu";
    const On1xSDK_TargetFeature sorted[]{{"avx2", 4}, {"sse2", 4}};
    auto equal_input = descriptor(canonical, sizeof(canonical) - 1, sorted, 2);
    auto second = create(equal_input);
    uint32_t same = 0;
    CHECK(on1x_sdk_target_equal(first.get(), second.get(), &same) == ON1X_SDK_OK);
    CHECK(same == 1);

    auto no_features_input = equal_input;
    no_features_input.struct_size =
        offsetof(On1xSDK_TargetDescriptor, features);
    no_features_input.features = reinterpret_cast<On1xSDK_TargetFeature *>(1);
    no_features_input.feature_count = 1;
    auto no_features = create(no_features_input);
    CHECK(on1x_sdk_target_has_feature(no_features.get(), "avx2", 4, &present) ==
          ON1X_SDK_OK);
    CHECK(present == 0);

    auto bad = equal_input;
    bad.abi_version = 1;
    CHECK(on1x_sdk_target_create(&bad, &raw) == ON1X_SDK_ABI_MISMATCH);
    bad = equal_input;
    bad.struct_size = 8;
    CHECK(on1x_sdk_target_create(&bad, &raw) == ON1X_SDK_ABI_MISMATCH);
    for (const char *text : {"X86_64-unknown-linux-gnu", "x86_64--linux-gnu",
                             "x86_64-linux", "x86_64-unknown-linux-gnu-extra-too"}) {
        bad = descriptor(text, std::strlen(text));
        CHECK(on1x_sdk_target_create(&bad, &raw) == ON1X_SDK_INVALID_ARGUMENT);
    }
    const char embedded[] = {'x', '8', '6', '\0', 'x', '-', 'u', '-', 'o'};
    bad = descriptor(embedded, sizeof embedded);
    CHECK(on1x_sdk_target_create(&bad, &raw) == ON1X_SDK_INVALID_ARGUMENT);

    bad = equal_input;
    bad.data_layout.pointer_bits = 32;
    CHECK(on1x_sdk_target_create(&bad, &raw) == ON1X_SDK_INVALID_ARGUMENT);
    bad = equal_input;
    bad.calling_convention = ON1X_SDK_CALLING_CONVENTION_C;
    CHECK(on1x_sdk_target_create(&bad, &raw) == ON1X_SDK_INVALID_ARGUMENT);
    bad = equal_input;
    bad.feature_count = ON1X_SDK_MAX_TARGET_FEATURES + 1;
    CHECK(on1x_sdk_target_create(&bad, &raw) == ON1X_SDK_LIMIT);
    const On1xSDK_TargetFeature duplicate[]{{"sse2", 4}, {"sse2", 4}};
    bad = descriptor(canonical, sizeof(canonical) - 1, duplicate, 2);
    CHECK(on1x_sdk_target_create(&bad, &raw) == ON1X_SDK_INVALID_ARGUMENT);

    auto layout = x64_layout();
    CHECK(on1x_sdk_data_layout_validate(&layout) == ON1X_SDK_OK);
    layout.stack_alignment = 24;
    CHECK(on1x_sdk_data_layout_validate(&layout) == ON1X_SDK_INVALID_ARGUMENT);
    layout = x64_layout();
    layout.struct_size = 8;
    CHECK(on1x_sdk_data_layout_validate(&layout) == ON1X_SDK_ABI_MISMATCH);

    uint32_t available = 1;
    for (uint32_t facility = ON1X_SDK_MACHINE_INSTRUCTION_SELECTION;
         facility <= ON1X_SDK_MACHINE_AOT; ++facility) {
        available = 1;
        CHECK(on1x_sdk_target_machine_probe(
                  first.get(), static_cast<On1xSDK_MachineFacility>(facility),
                  &available) == ON1X_SDK_OK);
        CHECK(available == 0);
    }
    CHECK(on1x_sdk_target_machine_probe(first.get(),
                                        static_cast<On1xSDK_MachineFacility>(99),
                                        &available) == ON1X_SDK_INVALID_ARGUMENT);
    On1xSDK_TargetMachine *machine = reinterpret_cast<On1xSDK_TargetMachine *>(1);
    On1xSDK_TargetMachineDescriptor machine_input{
        ON1X_SDK_TARGET_ABI_VERSION, sizeof(On1xSDK_TargetMachineDescriptor),
        first.get(), 2, 0};
    CHECK(on1x_sdk_target_machine_create(&machine_input, &machine) ==
          ON1X_SDK_UNSUPPORTED);
    CHECK(machine == nullptr);
    machine_input.struct_size =
        offsetof(On1xSDK_TargetMachineDescriptor, reserved);
    machine_input.reserved = 99;
    CHECK(on1x_sdk_target_machine_create(&machine_input, &machine) ==
          ON1X_SDK_UNSUPPORTED);
    CHECK(machine == nullptr);
    machine_input.struct_size = sizeof machine_input;
    CHECK(on1x_sdk_target_machine_create(&machine_input, &machine) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(machine == nullptr);

    On1xSDK_MachineIR *machine_ir = reinterpret_cast<On1xSDK_MachineIR *>(1);
    CHECK(on1x_sdk_machine_ir_create(nullptr, &machine_ir) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(machine_ir == nullptr);
    const auto *unavailable_machine =
        reinterpret_cast<const On1xSDK_TargetMachine *>(1);
    CHECK(on1x_sdk_machine_ir_create(unavailable_machine, &machine_ir) ==
          ON1X_SDK_UNSUPPORTED);
    CHECK(machine_ir == nullptr);
    On1xSDK_JIT *jit = reinterpret_cast<On1xSDK_JIT *>(1);
    CHECK(on1x_sdk_jit_create(unavailable_machine, &jit) ==
          ON1X_SDK_UNSUPPORTED);
    CHECK(jit == nullptr);
    On1xSDK_Artifact *artifact = reinterpret_cast<On1xSDK_Artifact *>(1);
    CHECK(on1x_sdk_aot_compile(
              unavailable_machine, reinterpret_cast<const On1xSDK_Program *>(1),
              &artifact) == ON1X_SDK_UNSUPPORTED);
    CHECK(artifact == nullptr);
    on1x_sdk_jit_destroy(nullptr);
    on1x_sdk_target_machine_destroy(nullptr);
    on1x_sdk_machine_ir_destroy(nullptr);
}
