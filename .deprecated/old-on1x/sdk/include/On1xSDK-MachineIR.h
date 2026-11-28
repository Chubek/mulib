/* Native machine IR is intentionally unavailable until verified legalization. */
#ifndef ON1X_SDK_MACHINE_IR_H
#define ON1X_SDK_MACHINE_IR_H

#include "On1xSDK-TargetMachine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_MachineIR On1xSDK_MachineIR;

/*
 * This fail-closed entry point exists so callers can distinguish unsupported
 * native lowering from allocation or malformed-input failure.
 */
On1xSDK_Status on1x_sdk_machine_ir_create(
    const On1xSDK_TargetMachine *machine, On1xSDK_MachineIR **out_ir);
void on1x_sdk_machine_ir_destroy(On1xSDK_MachineIR *ir);

#ifdef __cplusplus
}
#endif

#endif
