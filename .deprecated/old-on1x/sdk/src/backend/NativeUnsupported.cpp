#include "On1xSDK-AOT.h"
#include "On1xSDK-JIT.h"
#include "On1xSDK-MachineIR.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {

On1xSDK_Status on1x_sdk_target_machine_probe(
    const On1xSDK_Target *target, On1xSDK_MachineFacility facility,
    uint32_t *out_available) {
    if (!out_available) return ON1X_SDK_INVALID_ARGUMENT;
    *out_available = 0;
    if (!target || facility < ON1X_SDK_MACHINE_INSTRUCTION_SELECTION ||
        facility > ON1X_SDK_MACHINE_AOT)
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_target_machine_create(
    const On1xSDK_TargetMachineDescriptor *descriptor,
    On1xSDK_TargetMachine **out_machine) {
    if (!out_machine) return ON1X_SDK_INVALID_ARGUMENT;
    *out_machine = nullptr;
    if (!descriptor) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t abi{}, size{};
    std::memcpy(&abi, &descriptor->abi_version, sizeof abi);
    std::memcpy(&size, &descriptor->struct_size, sizeof size);
    if (abi != ON1X_SDK_TARGET_ABI_VERSION ||
        size < offsetof(On1xSDK_TargetMachineDescriptor, reserved))
        return ON1X_SDK_ABI_MISMATCH;
    On1xSDK_TargetMachineDescriptor input{};
    std::memcpy(&input, descriptor,
                size < sizeof input ? size : sizeof input);
    if (size < offsetof(On1xSDK_TargetMachineDescriptor, reserved) +
                   sizeof(input.reserved))
        input.reserved = 0;
    if (!input.target || input.optimization_level > 3u || input.reserved != 0)
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_UNSUPPORTED;
}

void on1x_sdk_target_machine_destroy(On1xSDK_TargetMachine *) {}

On1xSDK_Status on1x_sdk_machine_ir_create(
    const On1xSDK_TargetMachine *machine, On1xSDK_MachineIR **out_ir) {
    if (!out_ir) return ON1X_SDK_INVALID_ARGUMENT;
    *out_ir = nullptr;
    return machine ? ON1X_SDK_UNSUPPORTED : ON1X_SDK_INVALID_ARGUMENT;
}

void on1x_sdk_machine_ir_destroy(On1xSDK_MachineIR *) {}

On1xSDK_Status on1x_sdk_aot_compile(const On1xSDK_TargetMachine *machine,
                                    const On1xSDK_Program *program,
                                    On1xSDK_Artifact **out_artifact) {
    if (!out_artifact) return ON1X_SDK_INVALID_ARGUMENT;
    *out_artifact = nullptr;
    return machine && program ? ON1X_SDK_UNSUPPORTED : ON1X_SDK_INVALID_ARGUMENT;
}

On1xSDK_Status on1x_sdk_jit_create(const On1xSDK_TargetMachine *machine,
                                   On1xSDK_JIT **out_jit) {
    if (!out_jit) return ON1X_SDK_INVALID_ARGUMENT;
    *out_jit = nullptr;
    return machine ? ON1X_SDK_UNSUPPORTED : ON1X_SDK_INVALID_ARGUMENT;
}

void on1x_sdk_jit_destroy(On1xSDK_JIT *) {}

} // extern "C"
