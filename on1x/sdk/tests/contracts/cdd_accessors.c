#include <mulib-cdd-accessors.h>

#include <stdio.h>
#include <string.h>

static int failure(int line, const char *message) {
  fprintf(stderr, "cdd_accessors.c:%d: %s\n", line, message);
  return line;
}

#define CHECK(condition, message)                                              \
  do {                                                                         \
    if (!(condition))                                                          \
      return failure(__LINE__, message);                                       \
  } while (0)

int main(void) {
  static const mulib_cdd_cap_t capabilities[] = {{"ffi.scalar", 1u}};
  static const mulib_cdd_attr_t attributes[] = {
      {"reviewed", {MULIB_CDD_BOOL, 1, NULL}}};
  static const mulib_cdd_entry_t entries[] = {
      {"scalar", "ffi", "MULIB_IMPL_SCALAR", 1u, 7u, capabilities, 1u, NULL, 0u,
       NULL, 0u, attributes, 1u}};
  static const mulib_cdd_catalogue_t catalogue = {entries, 1u};
  static const mulib_cdd_req_t goals[] = {{"ffi.scalar", 1u, 1u}};
  static const mulib_cdd_problem_t problem = {goals, 1u,         NULL,
                                              0u,    UINT64_MAX, 7u};
  mulib_cdd_plan_t plan = {{0u}, 1u, 7u, 1};
  const mulib_cdd_entry_t *entry = NULL;
  const mulib_cdd_cap_t *capability = NULL;
  const mulib_cdd_req_t *requirement = (const mulib_cdd_req_t *)(uintptr_t)1u;
  const mulib_cdd_attr_t *attribute = NULL;
  const char *conflict = (const char *)(uintptr_t)1u;
  size_t count = 99u;
  size_t catalogue_index = MULIB_CDD_NONE;

  CHECK(mulib_cdd_catalogue_entry_count_checked(&catalogue, &count) ==
                MULIB_CDD_OK &&
            count == 1u,
        "catalogue count accessor failed");
  CHECK(mulib_cdd_catalogue_entry_at_checked(&catalogue, 0u, &entry) ==
                MULIB_CDD_OK &&
            entry == &entries[0],
        "catalogue entry accessor failed");
  CHECK(mulib_cdd_catalogue_entry_at_checked(&catalogue, 1u, &entry) ==
                MULIB_CDD_NOT_FOUND &&
            entry == NULL,
        "out-of-range entry accessor did not clear output");
  CHECK(mulib_cdd_entry_provide_count_checked(&catalogue, 0u, &count) ==
                MULIB_CDD_OK &&
            count == 1u,
        "provide count accessor failed");
  CHECK(mulib_cdd_entry_provide_at_checked(&catalogue, 0u, 0u, &capability) ==
                MULIB_CDD_OK &&
            capability == &capabilities[0],
        "provide accessor failed");
  CHECK(mulib_cdd_entry_requirement_count_checked(&catalogue, 0u, &count) ==
                MULIB_CDD_OK &&
            count == 0u,
        "requirement count accessor failed");
  CHECK(mulib_cdd_entry_requirement_at_checked(
            &catalogue, 0u, 0u, &requirement) == MULIB_CDD_NOT_FOUND &&
            requirement == NULL,
        "missing requirement did not clear output");
  CHECK(mulib_cdd_entry_conflict_count_checked(&catalogue, 0u, &count) ==
                MULIB_CDD_OK &&
            count == 0u,
        "conflict count accessor failed");
  CHECK(mulib_cdd_entry_conflict_at_checked(&catalogue, 0u, 0u, &conflict) ==
                MULIB_CDD_NOT_FOUND &&
            conflict == NULL,
        "missing conflict did not clear output");
  CHECK(mulib_cdd_entry_attr_count_checked(&catalogue, 0u, &count) ==
                MULIB_CDD_OK &&
            count == 1u,
        "attribute count accessor failed");
  CHECK(mulib_cdd_entry_attr_at_checked(&catalogue, 0u, 0u, &attribute) ==
                MULIB_CDD_OK &&
            attribute == &attributes[0],
        "attribute accessor failed");
  CHECK(mulib_cdd_plan_entry_count_checked(&catalogue, &problem, &plan,
                                           &count) == MULIB_CDD_OK &&
            count == 1u,
        "plan count accessor failed");
  CHECK(mulib_cdd_plan_entry_at_checked(&catalogue, &problem, &plan, 0u,
                                        &catalogue_index,
                                        &entry) == MULIB_CDD_OK &&
            catalogue_index == 0u && entry == &entries[0],
        "plan entry accessor failed");
  CHECK(mulib_cdd_plan_entry_at_checked(&catalogue, &problem, &plan, 1u,
                                        &catalogue_index,
                                        &entry) == MULIB_CDD_NOT_FOUND &&
            catalogue_index == MULIB_CDD_NONE && entry == NULL,
        "out-of-range plan accessor did not clear outputs");

  {
    const mulib_cdd_catalogue_t oversized = {entries,
                                             MULIB_CDD_MAX_ENTRIES + 1u};
    count = 99u;
    CHECK(mulib_cdd_catalogue_entry_count_checked(&oversized, &count) ==
                  MULIB_CDD_LIMIT &&
              count == 0u,
          "oversized catalogue was not rejected safely");
  }
  {
    mulib_cdd_entry_t malformed_entry = entries[0];
    const mulib_cdd_catalogue_t malformed = {&malformed_entry, 1u};
    malformed_entry.provides = NULL;
    capability = (const mulib_cdd_cap_t *)(uintptr_t)1u;
    CHECK(mulib_cdd_entry_provide_at_checked(&malformed, 0u, 0u, &capability) ==
                  MULIB_CDD_INVALID &&
              capability == NULL,
          "malformed nested array was not rejected safely");
  }
  {
    mulib_cdd_entry_t oversized_entry = entries[0];
    const mulib_cdd_catalogue_t oversized = {&oversized_entry, 1u};
    oversized_entry.provide_count = MULIB_CDD_ACCESS_MAX_ITEMS + 1u;
    count = 99u;
    CHECK(mulib_cdd_entry_provide_count_checked(&oversized, 0u, &count) ==
                  MULIB_CDD_LIMIT &&
              count == 0u,
          "oversized nested array was not bounded before validation");
  }
  {
    char oversized_text[MULIB_CDD_ACCESS_MAX_TEXT_BYTES + 2u];
    mulib_cdd_entry_t oversized_entry = entries[0];
    const mulib_cdd_catalogue_t oversized = {&oversized_entry, 1u};
    memset(oversized_text, 'a', sizeof(oversized_text));
    oversized_entry.id = oversized_text;
    count = 99u;
    CHECK(mulib_cdd_catalogue_entry_count_checked(&oversized, &count) ==
                  MULIB_CDD_LIMIT &&
              count == 0u,
          "unterminated catalogue text was not finitely bounded");
  }
  {
    mulib_cdd_problem_t oversized_problem = problem;
    oversized_problem.goal_count = MULIB_CDD_ACCESS_MAX_ITEMS + 1u;
    count = 99u;
    CHECK(mulib_cdd_plan_entry_count_checked(&catalogue, &oversized_problem,
                                             &plan,
                                             &count) == MULIB_CDD_LIMIT &&
              count == 0u,
          "oversized problem array was not bounded before validation");
  }
  plan.cost = 8u;
  count = 99u;
  CHECK(mulib_cdd_plan_entry_count_checked(&catalogue, &problem, &plan,
                                           &count) == MULIB_CDD_INVALID &&
            count == 0u,
        "malformed plan was not rejected safely");
  return 0;
}
