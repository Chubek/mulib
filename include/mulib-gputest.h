/** @file mulib-gputest.h
 * @brief Bounded GPU stress orchestration using a caller-supplied device driver.
 * Define MULIB_IMPL_GPUTEST once. No portable GPU command-set is assumed.
 */
#ifndef MULIB_GPUTEST_H
#define MULIB_GPUTEST_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    void *context;
    int (*submit)(void *, uint32_t seed, uint32_t work_items);
    /** Must enforce timeout_ms and quiesce submitted work even on failure. */
    int (*wait)(void *, uint32_t timeout_ms);
    /** Compare GPU result with an independent reference; returns zero on match. */
    int (*verify)(void *, uint32_t seed, uint32_t work_items);
    int (*temperature)(void *, unsigned *millidegrees_c);
    void (*cleanup)(void *);
} mulib_gputest_backend_t;
typedef struct { uint32_t iterations, work_items, timeout_ms; unsigned max_temperature; } mulib_gputest_config_t;
typedef struct { uint32_t completed; unsigned peak_temperature; int backend_status; } mulib_gputest_result_t;
/** @brief GLSL 4.30 compute workload, local size 64, SSBO binding 0 (uint output[]).
 * Set uint uniforms seed, count, rounds. Dispatch ceil(count/64) workgroups.
 * Synchronize shader writes before readback. All arithmetic wraps modulo 2^32.
 */
const char *mulib_gputest_shader(void);
/** @brief Independent CPU reference for one invocation of the supplied shader. */
uint32_t mulib_gputest_reference(uint32_t seed, uint32_t index, uint32_t rounds);
/** @brief Verify mapped shader results. Returns mismatch count, SIZE_MAX invalid. */
size_t mulib_gputest_verify(const uint32_t *values, size_t count, uint32_t seed, uint32_t rounds);
/** @brief Run finite submit/wait/verify cycles. 0 success, -1 invalid, -2 backend,
 * -3 thermal limit. Thermal limit requires a sensor. cleanup runs on all valid runs.
 * No software framebuffer loop is misrepresented as GPU execution.
 */
int mulib_gputest_run(const mulib_gputest_backend_t *backend, const mulib_gputest_config_t *config,
                     mulib_gputest_result_t *result);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GPUTEST) && !defined(MULIB_GPUTEST_IMPLEMENTED)
#define MULIB_GPUTEST_IMPLEMENTED
const char *mulib_gputest_shader(void) {
    return "#version 430\n"
           "layout(local_size_x=64) in;\n"
           "layout(std430,binding=0) buffer Results { uint values[]; };\n"
           "uniform uint seed, count, rounds;\n"
           "void main(){ uint i=gl_GlobalInvocationID.x; if(i>=count)return;\n"
           "uint x=seed^(i*2654435769u);\n"
           "for(uint j=0u;j<rounds;++j){x^=x>>16u;x*=2146121005u;"
           "x^=x>>15u;x*=2221713035u;x^=x>>16u;x+=j;}\n"
           "values[i]=x;}\n";
}
uint32_t mulib_gputest_reference(uint32_t seed, uint32_t index, uint32_t rounds) {
    uint32_t x = seed ^ (index * UINT32_C(2654435769)), j;
    for (j = 0; j < rounds; ++j) {
        x ^= x >> 16; x *= UINT32_C(2146121005);
        x ^= x >> 15; x *= UINT32_C(2221713035); x ^= x >> 16; x += j;
    }
    return x;
}
size_t mulib_gputest_verify(const uint32_t *v, size_t count, uint32_t seed, uint32_t rounds) {
    size_t i, errors = 0;
    if ((!v && count) || count > UINT32_MAX || count > SIZE_MAX / sizeof(*v)) return SIZE_MAX;
    for (i = 0; i < count; ++i) if (v[i] != mulib_gputest_reference(seed, (uint32_t)i, rounds)) ++errors;
    return errors;
}
int mulib_gputest_run(const mulib_gputest_backend_t *b, const mulib_gputest_config_t *c, mulib_gputest_result_t *r) {
    uint32_t i, seed; unsigned temp; int rc = 0;
    if (!r) return -1;
    r->completed = 0; r->peak_temperature = 0; r->backend_status = 0;
    if (!b || !c || !b->submit || !b->wait || !b->verify || !b->cleanup ||
        !c->iterations || !c->work_items || !c->timeout_ms || (c->max_temperature && !b->temperature)) return -1;
    for (i = 0; i < c->iterations; ++i) {
        if (b->temperature) {
            r->backend_status = b->temperature(b->context, &temp);
            if (r->backend_status) { rc = -2; break; }
            if (temp > r->peak_temperature) r->peak_temperature = temp;
            if (c->max_temperature && temp >= c->max_temperature) { rc = -3; break; }
        }
        seed = UINT32_C(0x9e3779b9) * (i + 1);
        r->backend_status = b->submit(b->context, seed, c->work_items);
        if (!r->backend_status) r->backend_status = b->wait(b->context, c->timeout_ms);
        if (!r->backend_status) r->backend_status = b->verify(b->context, seed, c->work_items);
        if (r->backend_status) { rc = -2; break; }
        ++r->completed;
        /* Include the last workload in thermal accounting too. */
        if (b->temperature) {
            r->backend_status = b->temperature(b->context, &temp);
            if (r->backend_status) { rc = -2; break; }
            if (temp > r->peak_temperature) r->peak_temperature = temp;
            if (c->max_temperature && temp >= c->max_temperature) { rc = -3; break; }
        }
    }
    b->cleanup(b->context); return rc;
}
#endif
