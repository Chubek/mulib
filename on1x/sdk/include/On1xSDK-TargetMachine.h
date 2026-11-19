/* Truthful target-machine capability probing. */
#ifndef ON1X_SDK_TARGET_MACHINE_H
#define ON1X_SDK_TARGET_MACHINE_H

#include "On1xSDK-Target.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_TargetMachine On1xSDK_TargetMachine;

typedef uint32_t On1xSDK_MachineFacility;
enum {
    ON1X_SDK_MACHINE_INSTRUCTION_SELECTION = 1u,
    ON1X_SDK_MACHINE_SCHEDULING = 2u,
    ON1X_SDK_MACHINE_REGISTER_ALLOCATION = 3u,
    ON1X_SDK_MACHINE_ASSEMBLY = 4u,
    ON1X_SDK_MACHINE_OBJECT_WRITING = 5u,
    ON1X_SDK_MACHINE_LINKING = 6u,
    ON1X_SDK_MACHINE_JIT = 7u,
    ON1X_SDK_MACHINE_AOT = 8u
};

typedef struct On1xSDK_TargetMachineDescriptor {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_Target *target;
    uint32_t optimization_level;
    uint32_t reserved;
} On1xSDK_TargetMachineDescriptor;

/*
 * Probe is executable and currently reports zero for every native facility.
 * The available MuLIB machine pipeline consumes a different IR and is not an
 * authorized bridge from verified On1x MIR.
 */
On1xSDK_Status on1x_sdk_target_machine_probe(
    const On1xSDK_Target *target, On1xSDK_MachineFacility facility,
    uint32_t *out_available);
/* Clears *out_machine and returns UNSUPPORTED for the current SDK profile. */
On1xSDK_Status on1x_sdk_target_machine_create(
    const On1xSDK_TargetMachineDescriptor *descriptor,
    On1xSDK_TargetMachine **out_machine);
void on1x_sdk_target_machine_destroy(On1xSDK_TargetMachine *machine);

#ifdef __cplusplus
}
#endif

#endif
