#ifndef ON1X_KERNEL_RECIPE_H
#define ON1X_KERNEL_RECIPE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_KERNEL_RECIPE_MAX_BYTES 65536u
#define ON1X_KERNEL_RECIPE_MAX_EXPORTS 16u
#define ON1X_KERNEL_RECIPE_MAX_DEPTH 32u /* Expression root is at depth zero. */
#define ON1X_KERNEL_RECIPE_MAX_EXPR_NODES 512u
#define ON1X_KERNEL_RECIPE_MAX_NODES (2u * ON1X_KERNEL_RECIPE_MAX_EXPR_NODES)

enum on1x_kernel_recipe_op {
    ON1X_KERNEL_OP_BOOL, ON1X_KERNEL_OP_PARAM, ON1X_KERNEL_OP_INTEGER,
    ON1X_KERNEL_OP_LT, ON1X_KERNEL_OP_LE, ON1X_KERNEL_OP_GT, ON1X_KERNEL_OP_GE,
    ON1X_KERNEL_OP_EQ, ON1X_KERNEL_OP_AND, ON1X_KERNEL_OP_OR,
    ON1X_KERNEL_OP_ADD, ON1X_KERNEL_OP_SUB, ON1X_KERNEL_OP_IF
};

struct on1x_kernel_recipe_node {
    enum on1x_kernel_recipe_op op;
    uint16_t a, b, c;
    int64_t immediate;
};

/* Zero-based postorder nodes; guard and body are disjoint trees. Used child
 * indices are strictly smaller than their parent. immediate is used only for
 * bool (0/1), parameter (zero-based), and integer leaves. Treat as read-only
 * after construction. Verification here checks syntax/types, not arithmetic
 * totality or authorization; compare to an independent, trusted behavior pin
 * before making an export callable. */
struct on1x_kernel_verified_export {
    char member[65], symbol[65], behavior_sha256[65];
    size_t arity, node_count;
    uint16_t guard, body;
    struct on1x_kernel_recipe_node nodes[ON1X_KERNEL_RECIPE_MAX_NODES];
};

struct on1x_kernel_recipe {
    char kernel[129], header[137], source_sha256[65];
    size_t export_count;
    struct on1x_kernel_verified_export exports[ON1X_KERNEL_RECIPE_MAX_EXPORTS];
};

struct on1x_kernel_recipe_error {
    size_t offset;
    const char *message; /* Static message; never points into input. */
};

/* Parse the bounded late-load subset of ffispec 1 (only i64 arities 2 and 3).
 * Returns 1 on success and 0 on failure. The caller owns out (heap allocation
 * is recommended); it borrows no bytes. On failure its contents are unusable.
 * bytes, out, and optional error must not overlap. The source_sha256 is
 * provenance text from the payload, not verified evidence of any identity.
 *
 * Each behavior_sha256 hashes ASCII "(guard EXPR)\n(body EXPR)\n", with
 * canonical decimal i64 literals, $0..$2 for parameters, and one space
 * between operator/operands in each S-expression. This excludes metadata,
 * selectors and optimization directives; check the interface separately.
 * Directives are validated as syntax only and never applied. */
int on1x_kernel_recipe_parse(const void *bytes, size_t size,
                             struct on1x_kernel_recipe *out,
                             struct on1x_kernel_recipe_error *error);

int on1x_kernel_sha256(const void *bytes, size_t size, char out[65]);

#ifdef __cplusplus
}
#endif
#endif
