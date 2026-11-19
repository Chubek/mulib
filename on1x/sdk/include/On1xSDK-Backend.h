/* Capability-selected artifact callbacks for host-admitted representations. */
#ifndef ON1X_SDK_BACKEND_H
#define ON1X_SDK_BACKEND_H

#include "On1xSDK-Artifact.h"
#include "On1xSDK-Target.h"
#include "On1xSDK-Transformer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ABI-v1 compatibility descriptor. Its layout and behavior are frozen.
 * New providers use On1xSDK_BackendProvider below.
 */
typedef On1xSDK_Status (*On1xSDK_BackendFn)(void *userdata,
                                         const On1xSDK_Program *program,
                                         On1xSDK_OutputSink *artifact);

typedef struct On1xSDK_Backend {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    On1xSDK_Phase phase; /* VERIFIED_MIR only. */
    On1xSDK_BackendFn run;
    void *userdata;      /* Borrowed for the registration's lifetime. */
} On1xSDK_Backend;

/* Uses the same ABI/ownership rules as on1x_sdk_analyzer_register. */
On1xSDK_Status on1x_sdk_backend_register(On1xSDK_Registry *registry,
                                         const On1xSDK_Backend *backend);
/*
 * Runs one named backend on host-verified MIR, exposed as canonical
 * serialized MIR bytes. The callback borrows program; it must not mutate,
 * destroy, or retain it. Missing name or mismatched phase is UNSUPPORTED.
 * The byte limit cannot exceed ON1X_SDK_MAX_ARTIFACT_BYTES. No partial
 * artifact is returned; *out is NULL on any failure. Success transfers
 * ownership of an opaque byte artifact (it may be empty). The SDK makes no
 * claim about its format or target and does not execute or publish it.
 */
On1xSDK_Status on1x_sdk_backend_run(const On1xSDK_Registry *registry,
                                    const char *name,
                                    const On1xSDK_Program *program,
                                    size_t byte_limit, On1xSDK_Artifact **out);

/* SDK 2 capability-driven providers. */
#define ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION 2u
#define ON1X_SDK_MAX_BACKEND_CAPABILITIES 32u
#define ON1X_SDK_MAX_BACKEND_TARGETS 32u
#define ON1X_SDK_MAX_BACKEND_FEATURES 32u

enum {
    ON1X_SDK_BACKEND_TARGET_INDEPENDENT = 1u << 0,
    ON1X_SDK_BACKEND_DETERMINISTIC = 1u << 1
};

typedef struct On1xSDK_BackendTarget {
    const char *triple;
    size_t triple_size;
} On1xSDK_BackendTarget;

typedef struct On1xSDK_BackendCapability {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_Phase input_phase;
    uint32_t input_schema_min;
    uint32_t input_schema_max;
    On1xSDK_ArtifactKind artifact_kind;
    uint32_t flags;

    /* Exact canonical triples; empty only for TARGET_INDEPENDENT output. */
    const On1xSDK_BackendTarget *targets;
    size_t target_count;
    const On1xSDK_TargetFeature *required_features;
    size_t required_feature_count;
    const On1xSDK_TargetFeature *forbidden_features;
    size_t forbidden_feature_count;
} On1xSDK_BackendCapability;

typedef struct On1xSDK_BackendInvocation On1xSDK_BackendInvocation;

/*
 * A poll callback is synchronous and borrowed for one invocation. OK continues.
 * Any other valid SDK status stops work and discards all staged output.
 */
typedef On1xSDK_Status (*On1xSDK_BackendPollFn)(void *userdata);
typedef On1xSDK_Status (*On1xSDK_BackendProviderFn)(
    void *userdata,
    const On1xSDK_BackendInvocation *invocation,
    On1xSDK_OutputSink *artifact
);

typedef struct On1xSDK_BackendProvider {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    size_t name_size;
    int32_t priority;
    uint32_t reserved;
    const On1xSDK_BackendCapability *capabilities;
    size_t capability_count;
    On1xSDK_BackendProviderFn run;
    void *userdata; /* Borrowed until registry destruction. */
} On1xSDK_BackendProvider;

struct On1xSDK_BackendInvocation {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_Program *program; /* Borrowed host-admitted input. */
    const On1xSDK_Target *target;   /* NULL only for target-independent output. */
    On1xSDK_ArtifactKind artifact_kind;
    uint32_t input_schema;
    size_t capability_index;
    size_t byte_limit;
    On1xSDK_BackendPollFn poll;
    void *poll_userdata;
};

enum {
    ON1X_SDK_BACKEND_REQUIRE_DETERMINISTIC = 1u << 0,
    ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY = 1u << 1
};

typedef struct On1xSDK_BackendRequest {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_Program *program;
    const On1xSDK_Target *target;
    On1xSDK_ArtifactKind artifact_kind;
    uint32_t input_schema; /* Zero asks the host to derive the canonical schema. */
    const char *provider_name; /* Optional exact registration name. */
    size_t provider_name_size;
    uint32_t flags;
    uint32_t reserved;
    size_t byte_limit;
    On1xSDK_BackendPollFn poll;
    void *poll_userdata;
} On1xSDK_BackendRequest;

/*
 * Registration deep-copies descriptor strings/arrays. Public providers receive
 * serialized VERIFIED_MIR only. Checked-HIR adapters remain trusted built-ins.
 * Native assembly/object/executable capabilities reject as UNSUPPORTED until a
 * verified On1x machine pipeline is available.
 */
On1xSDK_Status on1x_sdk_backend_provider_register(
    On1xSDK_Registry *registry, const On1xSDK_BackendProvider *provider);

/*
 * Deterministically selects the unique highest-priority compatible provider,
 * runs it synchronously into a private staging sink, and returns no artifact on
 * failure. Equal best-ranked providers are an ambiguity and return DUPLICATE.
 */
On1xSDK_Status on1x_sdk_backend_produce(const On1xSDK_Registry *registry,
                                        const On1xSDK_BackendRequest *request,
                                        On1xSDK_Artifact **out_artifact);

/* Polls through a borrowed invocation, normalizing unknown callback statuses. */
On1xSDK_Status on1x_sdk_backend_poll(const On1xSDK_BackendInvocation *invocation);

#ifdef __cplusplus
}
#endif

#endif
