#ifndef ON1X_SDK_SERIALIZATION_H
#define ON1X_SDK_SERIALIZATION_H

#include "On1xSDK-String.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_SERIAL_ENVELOPE_MAGIC UINT32_C(0x4f315853) /* O1XS */

/*
 * Pointer-free wire prefix. Integer fields are encoded big-endian by the
 * helpers below; the native structure itself is not serialized with memcpy.
 */
typedef struct On1xSDK_SerializationEnvelope {
    uint32_t magic;
    uint16_t schema_major;
    uint16_t schema_minor;
    uint32_t flags;
    uint64_t revision;
    uint64_t payload_size;
    uint8_t digest_sha256[32];
} On1xSDK_SerializationEnvelope;

#define ON1X_SDK_SERIALIZATION_ENVELOPE_WIRE_SIZE 60u

On1xSDK_Status on1x_sdk_serialization_envelope_encode(
    const On1xSDK_SerializationEnvelope *envelope,
    void *output,
    size_t output_size
);
On1xSDK_Status on1x_sdk_serialization_envelope_decode(
    const void *input,
    size_t input_size,
    uint16_t supported_major,
    uint64_t maximum_payload,
    On1xSDK_SerializationEnvelope *out
);

#ifdef __cplusplus
}
#endif

#endif
