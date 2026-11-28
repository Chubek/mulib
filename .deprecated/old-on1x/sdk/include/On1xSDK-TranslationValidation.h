/* Finite evaluator evidence, never a general equivalence certificate. */
#ifndef ON1X_SDK_TRANSLATION_VALIDATION_H
#define ON1X_SDK_TRANSLATION_VALIDATION_H

#include "On1xSDK-PassManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_TranslationEvidenceKind;
enum {
    ON1X_SDK_TRANSLATION_EVIDENCED_EQUAL = 1,
    ON1X_SDK_TRANSLATION_EVIDENCED_DIFFERENT = 2
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *function_name;
    size_t function_name_size;
    const int64_t *scalar_arguments;
    size_t scalar_argument_count;
    uint64_t evaluator_step_limit;
    uint64_t evaluator_call_depth_limit;
} On1xSDK_TranslationTest;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_TranslationEvidenceKind evidence;
    uint32_t is_equivalence_certificate; /* Always zero. */
    uint64_t tests_executed;              /* Exactly one on success. */
    On1xSDK_AnalysisStringView subset;
} On1xSDK_TranslationEvidence;

On1xSDK_Status on1x_sdk_translation_test(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_PassResult *result,
    const On1xSDK_TranslationTest *test,
    On1xSDK_TranslationEvidence *out_evidence);

#ifdef __cplusplus
}
#endif

#endif
