/**
 * @file mulib-cdd-accessors.h
 * @brief Checked, count-explicit borrowed views over fixed-array CDD records.
 *
 * ISO C11; define MULIB_IMPL_CDD_ACCESSORS in exactly one translation unit.
 * This is an accessor layer, not a semantic contract or callable On1x binding.
 * Returned pointers borrow the catalogue descriptor and all of its nested data.
 * Registry mutation invalidates every pointer and plan index obtained here.
 * Nested arrays and NUL-terminated text are preflighted against the finite
 * limits below before the canonical CDD validators traverse them.
 */
#ifndef MULIB_CDD_ACCESSORS_H
#define MULIB_CDD_ACCESSORS_H

#include <stddef.h>
#include <stdint.h>

#include <mulib-cdd-plan.h>

#define MULIB_CDD_ACCESS_MAX_ITEMS 256u
#define MULIB_CDD_ACCESS_MAX_TEXT_BYTES 4096u

#ifdef __cplusplus
extern "C" {
#endif

mulib_cdd_status_t
mulib_cdd_catalogue_entry_count_checked(const mulib_cdd_catalogue_t *catalogue,
                                        size_t *count);
mulib_cdd_status_t
mulib_cdd_catalogue_entry_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                     size_t index,
                                     const mulib_cdd_entry_t **entry);

mulib_cdd_status_t
mulib_cdd_entry_provide_count_checked(const mulib_cdd_catalogue_t *catalogue,
                                      size_t entry_index, size_t *count);
mulib_cdd_status_t
mulib_cdd_entry_provide_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                   size_t entry_index, size_t item_index,
                                   const mulib_cdd_cap_t **capability);
mulib_cdd_status_t mulib_cdd_entry_requirement_count_checked(
    const mulib_cdd_catalogue_t *catalogue, size_t entry_index, size_t *count);
mulib_cdd_status_t
mulib_cdd_entry_requirement_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                       size_t entry_index, size_t item_index,
                                       const mulib_cdd_req_t **requirement);
mulib_cdd_status_t
mulib_cdd_entry_conflict_count_checked(const mulib_cdd_catalogue_t *catalogue,
                                       size_t entry_index, size_t *count);
mulib_cdd_status_t
mulib_cdd_entry_conflict_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                    size_t entry_index, size_t item_index,
                                    const char **conflict);
mulib_cdd_status_t
mulib_cdd_entry_attr_count_checked(const mulib_cdd_catalogue_t *catalogue,
                                   size_t entry_index, size_t *count);
mulib_cdd_status_t
mulib_cdd_entry_attr_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                size_t entry_index, size_t item_index,
                                const mulib_cdd_attr_t **attribute);

mulib_cdd_status_t
mulib_cdd_plan_entry_count_checked(const mulib_cdd_catalogue_t *catalogue,
                                   const mulib_cdd_problem_t *problem,
                                   const mulib_cdd_plan_t *plan, size_t *count);
mulib_cdd_status_t mulib_cdd_plan_entry_at_checked(
    const mulib_cdd_catalogue_t *catalogue, const mulib_cdd_problem_t *problem,
    const mulib_cdd_plan_t *plan, size_t order_index, size_t *catalogue_index,
    const mulib_cdd_entry_t **entry);

#ifdef __cplusplus
}
#endif

#endif /* MULIB_CDD_ACCESSORS_H */

#if defined(MULIB_IMPL_CDD_ACCESSORS) &&                                       \
    !defined(MULIB_CDD_ACCESSORS_IMPLEMENTED)
#define MULIB_CDD_ACCESSORS_IMPLEMENTED

#ifdef __cplusplus
extern "C" {
#endif

static mulib_cdd_status_t
mulib_cdd_access_validate_(const mulib_cdd_catalogue_t *catalogue);

static mulib_cdd_status_t mulib_cdd_access_text_(const char *text,
                                                 int allow_empty) {
  if (!text || (!allow_empty && !*text))
    return MULIB_CDD_INVALID;
  for (size_t index = 0; index <= MULIB_CDD_ACCESS_MAX_TEXT_BYTES; ++index)
    if (!text[index])
      return MULIB_CDD_OK;
  return MULIB_CDD_LIMIT;
}

static mulib_cdd_status_t
mulib_cdd_access_entry_(const mulib_cdd_catalogue_t *catalogue, size_t index,
                        const mulib_cdd_entry_t **entry) {
  mulib_cdd_status_t status;
  if (entry)
    *entry = NULL;
  if (!entry)
    return MULIB_CDD_INVALID;
  status = mulib_cdd_access_validate_(catalogue);
  if (status != MULIB_CDD_OK)
    return status;
  if (index >= catalogue->count)
    return MULIB_CDD_NOT_FOUND;
  *entry = catalogue->entries + index;
  return MULIB_CDD_OK;
}

static mulib_cdd_status_t
mulib_cdd_access_validate_(const mulib_cdd_catalogue_t *catalogue) {
  if (!catalogue || (!catalogue->entries && catalogue->count))
    return MULIB_CDD_INVALID;
  if (catalogue->count > MULIB_CDD_MAX_ENTRIES)
    return MULIB_CDD_LIMIT;
  for (size_t index = 0; index < catalogue->count; ++index) {
    const mulib_cdd_entry_t *entry = catalogue->entries + index;
    mulib_cdd_status_t status;
    if (entry->provide_count > MULIB_CDD_ACCESS_MAX_ITEMS ||
        entry->require_count > MULIB_CDD_ACCESS_MAX_ITEMS ||
        entry->conflict_count > MULIB_CDD_ACCESS_MAX_ITEMS ||
        entry->attr_count > MULIB_CDD_ACCESS_MAX_ITEMS)
      return MULIB_CDD_LIMIT;
    if ((!entry->provides && entry->provide_count) ||
        (!entry->requirements && entry->require_count) ||
        (!entry->conflicts && entry->conflict_count) ||
        (!entry->attrs && entry->attr_count))
      return MULIB_CDD_INVALID;
    status = mulib_cdd_access_text_(entry->id, 0);
    if (status != MULIB_CDD_OK)
      return status;
    status = mulib_cdd_access_text_(entry->category, 0);
    if (status != MULIB_CDD_OK)
      return status;
    status = mulib_cdd_access_text_(entry->implementation, 0);
    if (status != MULIB_CDD_OK)
      return status;
    for (size_t item = 0; item < entry->provide_count; ++item) {
      status = mulib_cdd_access_text_(entry->provides[item].name, 0);
      if (status != MULIB_CDD_OK)
        return status;
    }
    for (size_t item = 0; item < entry->require_count; ++item) {
      status = mulib_cdd_access_text_(entry->requirements[item].name, 0);
      if (status != MULIB_CDD_OK)
        return status;
    }
    for (size_t item = 0; item < entry->conflict_count; ++item) {
      status = mulib_cdd_access_text_(entry->conflicts[item], 0);
      if (status != MULIB_CDD_OK)
        return status;
    }
    for (size_t item = 0; item < entry->attr_count; ++item) {
      const mulib_cdd_attr_t *attribute = entry->attrs + item;
      status = mulib_cdd_access_text_(attribute->key, 0);
      if (status != MULIB_CDD_OK)
        return status;
      if (attribute->value.kind == MULIB_CDD_STRING) {
        status = mulib_cdd_access_text_(attribute->value.string, 1);
        if (status != MULIB_CDD_OK)
          return status;
      }
    }
  }
  return mulib_cdd_validate(catalogue, NULL);
}

static mulib_cdd_status_t
mulib_cdd_access_plan_validate_(const mulib_cdd_catalogue_t *catalogue,
                                const mulib_cdd_problem_t *problem,
                                const mulib_cdd_plan_t *plan) {
  mulib_cdd_status_t status = mulib_cdd_access_validate_(catalogue);
  if (status != MULIB_CDD_OK)
    return status;
  if (!problem || (!problem->goals && problem->goal_count) ||
      (!problem->available && problem->available_count))
    return MULIB_CDD_INVALID;
  if (problem->goal_count > MULIB_CDD_ACCESS_MAX_ITEMS ||
      problem->available_count > MULIB_CDD_ACCESS_MAX_ITEMS)
    return MULIB_CDD_LIMIT;
  for (size_t index = 0; index < problem->goal_count; ++index) {
    status = mulib_cdd_access_text_(problem->goals[index].name, 0);
    if (status != MULIB_CDD_OK)
      return status;
  }
  for (size_t index = 0; index < problem->available_count; ++index) {
    status = mulib_cdd_access_text_(problem->available[index].name, 0);
    if (status != MULIB_CDD_OK)
      return status;
  }
  return mulib_cdd_plan_validate(catalogue, problem, plan, NULL);
}

mulib_cdd_status_t
mulib_cdd_catalogue_entry_count_checked(const mulib_cdd_catalogue_t *catalogue,
                                        size_t *count) {
  mulib_cdd_status_t status;
  if (count)
    *count = 0;
  if (!count)
    return MULIB_CDD_INVALID;
  status = mulib_cdd_access_validate_(catalogue);
  if (status != MULIB_CDD_OK)
    return status;
  *count = catalogue->count;
  return MULIB_CDD_OK;
}

mulib_cdd_status_t
mulib_cdd_catalogue_entry_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                     size_t index,
                                     const mulib_cdd_entry_t **entry) {
  return mulib_cdd_access_entry_(catalogue, index, entry);
}

#define MULIB_CDD_ACCESS_COUNT_(name, field)                                   \
  mulib_cdd_status_t name(const mulib_cdd_catalogue_t *catalogue,              \
                          size_t entry_index, size_t *count) {                 \
    const mulib_cdd_entry_t *entry = NULL;                                     \
    mulib_cdd_status_t status;                                                 \
    if (count)                                                                 \
      *count = 0;                                                              \
    if (!count)                                                                \
      return MULIB_CDD_INVALID;                                                \
    status = mulib_cdd_access_entry_(catalogue, entry_index, &entry);          \
    if (status != MULIB_CDD_OK)                                                \
      return status;                                                           \
    *count = entry->field;                                                     \
    return MULIB_CDD_OK;                                                       \
  }

MULIB_CDD_ACCESS_COUNT_(mulib_cdd_entry_provide_count_checked, provide_count)
MULIB_CDD_ACCESS_COUNT_(mulib_cdd_entry_requirement_count_checked,
                        require_count)
MULIB_CDD_ACCESS_COUNT_(mulib_cdd_entry_conflict_count_checked, conflict_count)
MULIB_CDD_ACCESS_COUNT_(mulib_cdd_entry_attr_count_checked, attr_count)

#undef MULIB_CDD_ACCESS_COUNT_

mulib_cdd_status_t
mulib_cdd_entry_provide_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                   size_t entry_index, size_t item_index,
                                   const mulib_cdd_cap_t **capability) {
  const mulib_cdd_entry_t *entry = NULL;
  mulib_cdd_status_t status;
  if (capability)
    *capability = NULL;
  if (!capability)
    return MULIB_CDD_INVALID;
  status = mulib_cdd_access_entry_(catalogue, entry_index, &entry);
  if (status != MULIB_CDD_OK)
    return status;
  if (item_index >= entry->provide_count)
    return MULIB_CDD_NOT_FOUND;
  *capability = entry->provides + item_index;
  return MULIB_CDD_OK;
}

mulib_cdd_status_t
mulib_cdd_entry_requirement_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                       size_t entry_index, size_t item_index,
                                       const mulib_cdd_req_t **requirement) {
  const mulib_cdd_entry_t *entry = NULL;
  mulib_cdd_status_t status;
  if (requirement)
    *requirement = NULL;
  if (!requirement)
    return MULIB_CDD_INVALID;
  status = mulib_cdd_access_entry_(catalogue, entry_index, &entry);
  if (status != MULIB_CDD_OK)
    return status;
  if (item_index >= entry->require_count)
    return MULIB_CDD_NOT_FOUND;
  *requirement = entry->requirements + item_index;
  return MULIB_CDD_OK;
}

mulib_cdd_status_t
mulib_cdd_entry_conflict_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                    size_t entry_index, size_t item_index,
                                    const char **conflict) {
  const mulib_cdd_entry_t *entry = NULL;
  mulib_cdd_status_t status;
  if (conflict)
    *conflict = NULL;
  if (!conflict)
    return MULIB_CDD_INVALID;
  status = mulib_cdd_access_entry_(catalogue, entry_index, &entry);
  if (status != MULIB_CDD_OK)
    return status;
  if (item_index >= entry->conflict_count)
    return MULIB_CDD_NOT_FOUND;
  *conflict = entry->conflicts[item_index];
  return MULIB_CDD_OK;
}

mulib_cdd_status_t
mulib_cdd_entry_attr_at_checked(const mulib_cdd_catalogue_t *catalogue,
                                size_t entry_index, size_t item_index,
                                const mulib_cdd_attr_t **attribute) {
  const mulib_cdd_entry_t *entry = NULL;
  mulib_cdd_status_t status;
  if (attribute)
    *attribute = NULL;
  if (!attribute)
    return MULIB_CDD_INVALID;
  status = mulib_cdd_access_entry_(catalogue, entry_index, &entry);
  if (status != MULIB_CDD_OK)
    return status;
  if (item_index >= entry->attr_count)
    return MULIB_CDD_NOT_FOUND;
  *attribute = entry->attrs + item_index;
  return MULIB_CDD_OK;
}

mulib_cdd_status_t mulib_cdd_plan_entry_count_checked(
    const mulib_cdd_catalogue_t *catalogue, const mulib_cdd_problem_t *problem,
    const mulib_cdd_plan_t *plan, size_t *count) {
  mulib_cdd_status_t status;
  if (count)
    *count = 0;
  if (!count)
    return MULIB_CDD_INVALID;
  status = mulib_cdd_access_plan_validate_(catalogue, problem, plan);
  if (status != MULIB_CDD_OK)
    return status;
  *count = plan->count;
  return MULIB_CDD_OK;
}

mulib_cdd_status_t mulib_cdd_plan_entry_at_checked(
    const mulib_cdd_catalogue_t *catalogue, const mulib_cdd_problem_t *problem,
    const mulib_cdd_plan_t *plan, size_t order_index, size_t *catalogue_index,
    const mulib_cdd_entry_t **entry) {
  mulib_cdd_status_t status;
  if (catalogue_index)
    *catalogue_index = MULIB_CDD_NONE;
  if (entry)
    *entry = NULL;
  if (!catalogue_index || !entry)
    return MULIB_CDD_INVALID;
  status = mulib_cdd_access_plan_validate_(catalogue, problem, plan);
  if (status != MULIB_CDD_OK)
    return status;
  if (order_index >= plan->count)
    return MULIB_CDD_NOT_FOUND;
  *catalogue_index = plan->order[order_index];
  *entry = catalogue->entries + *catalogue_index;
  return MULIB_CDD_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* MULIB_IMPL_CDD_ACCESSORS */
