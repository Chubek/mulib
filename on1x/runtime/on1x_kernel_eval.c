#include "on1x_kernel_eval.h"

#include "on1x_bounds.h"
#include "on1x_kernel_recipe.h"

struct eval_state {
    const struct on1x_kernel_verified_export *recipe;
    const int64_t *args;
    size_t remaining;
};

/* A child's index must be smaller than its parent's in the verifier's
 * postorder arena. Guard and body are disjoint trees, so their combined
 * execution cannot visit more nodes than the arena contains. */
static int eval_node(struct eval_state *state, uint16_t index,
                     size_t parent, unsigned depth, int64_t *out) {
    const struct on1x_kernel_recipe_node *node;
    int64_t a, b;
    int status;

    if (index >= parent || depth > ON1X_KERNEL_RECIPE_MAX_DEPTH || !state->remaining)
        return ON1X_KERNEL_EVAL_INVALID;
    --state->remaining;
    node = &state->recipe->nodes[index];
    switch (node->op) {
    case ON1X_KERNEL_OP_BOOL:
    case ON1X_KERNEL_OP_INTEGER:
        *out = node->immediate;
        return ON1X_KERNEL_EVAL_OK;
    case ON1X_KERNEL_OP_PARAM:
        if (node->immediate < 0 || (uint64_t)node->immediate >= state->recipe->arity)
            return ON1X_KERNEL_EVAL_INVALID;
        *out = state->args[(size_t)node->immediate];
        return ON1X_KERNEL_EVAL_OK;
    case ON1X_KERNEL_OP_LT:
    case ON1X_KERNEL_OP_LE:
    case ON1X_KERNEL_OP_GT:
    case ON1X_KERNEL_OP_GE:
    case ON1X_KERNEL_OP_EQ:
    case ON1X_KERNEL_OP_AND:
    case ON1X_KERNEL_OP_OR:
    case ON1X_KERNEL_OP_ADD:
    case ON1X_KERNEL_OP_SUB:
    case ON1X_KERNEL_OP_IF:
        break;
    default:
        return ON1X_KERNEL_EVAL_INVALID;
    }

    status = eval_node(state, node->a, index, depth + 1, &a);
    if (status != ON1X_KERNEL_EVAL_OK)
        return status;
    /* The verifier supplies booleans as 0/1. Never evaluate an unselected
     * operand: even an otherwise valid expression might overflow there. */
    if (node->op == ON1X_KERNEL_OP_IF)
        return eval_node(state, a ? node->b : node->c, index, depth + 1, out);
    if ((node->op == ON1X_KERNEL_OP_AND && !a) ||
        (node->op == ON1X_KERNEL_OP_OR && a)) {
        *out = a;
        return ON1X_KERNEL_EVAL_OK;
    }

    status = eval_node(state, node->b, index, depth + 1, &b);
    if (status != ON1X_KERNEL_EVAL_OK)
        return status;
    switch (node->op) {
    case ON1X_KERNEL_OP_LT:  *out = a < b; break;
    case ON1X_KERNEL_OP_LE:  *out = a <= b; break;
    case ON1X_KERNEL_OP_GT:  *out = a > b; break;
    case ON1X_KERNEL_OP_GE:  *out = a >= b; break;
    case ON1X_KERNEL_OP_EQ:  *out = a == b; break;
    case ON1X_KERNEL_OP_AND:
    case ON1X_KERNEL_OP_OR:  *out = b; break;
    case ON1X_KERNEL_OP_ADD:
        if (!on1x_try_add_i64(a, b, out))
            return ON1X_KERNEL_EVAL_OVERFLOW;
        break;
    case ON1X_KERNEL_OP_SUB:
        if (!on1x_try_sub_i64(a, b, out))
            return ON1X_KERNEL_EVAL_OVERFLOW;
        break;
    default:
        return ON1X_KERNEL_EVAL_INVALID;
    }
    return ON1X_KERNEL_EVAL_OK;
}

int on1x_kernel_eval(const struct on1x_kernel_verified_export *recipe,
                     const int64_t *args, size_t arity, int64_t *out) {
    int64_t value;
    int status;

    if (!recipe || !out || !args || (arity != 2 && arity != 3) ||
        recipe->arity != arity ||
        recipe->node_count > sizeof(recipe->nodes) / sizeof(recipe->nodes[0]) ||
        recipe->guard >= recipe->node_count || recipe->body >= recipe->node_count)
        return ON1X_KERNEL_EVAL_INVALID;

    struct eval_state state = {recipe, args, recipe->node_count};
    status = eval_node(&state, recipe->guard, recipe->node_count, 0, &value);
    if (status != ON1X_KERNEL_EVAL_OK)
        return status;
    if (value == 0)
        return ON1X_KERNEL_EVAL_GUARD;
    status = eval_node(&state, recipe->body, recipe->node_count, 0, &value);
    if (status == ON1X_KERNEL_EVAL_OK)
        *out = value;
    return status;
}
