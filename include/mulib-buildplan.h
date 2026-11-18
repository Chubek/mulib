/**
 * @file mulib-buildplan.h
 * @brief Bounded target DAG resolution and resource-aware build planning.
 *
 * Define MULIB_IMPL_BUILDPLAN in exactly one C11 translation unit.  BuildPlan
 * owns target names and tags, but only borrows optional application actions.
 * It plans work; process execution and artifact materialization are separate.
 */
#ifndef MULIB_BUILDPLAN_H
#define MULIB_BUILDPLAN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_buildplan mulib_buildplan_t;

typedef enum mulib_buildplan_status {
  MULIB_BUILDPLAN_OK = 0,
  MULIB_BUILDPLAN_INVALID = -1,
  MULIB_BUILDPLAN_MEMORY = -2,
  MULIB_BUILDPLAN_FULL = -3,
  MULIB_BUILDPLAN_CYCLE = -4,
  MULIB_BUILDPLAN_OVERFLOW = -5,
  MULIB_BUILDPLAN_BUSY = -6
} mulib_buildplan_status_t;

/** Borrowed target view. name is always nonempty; tag and action may be NULL. */
typedef struct mulib_buildplan_target {
  const char *name;
  const char *tag;
  const void *action;
  unsigned cpu;
  unsigned memory_mb;
  int priority;
  double duration;
} mulib_buildplan_target_t;

typedef struct mulib_buildplan_budget {
  unsigned cpu;       /**< Must be positive. */
  unsigned memory_mb; /**< Zero means no memory limit. */
} mulib_buildplan_budget_t;

/** The IDs array is borrowed for the callback and sorted by priority/name. */
typedef int (*mulib_buildplan_wave_fn)(void *user, const size_t *ids,
                                       size_t count);

/** Allocate fixed target and dependency capacities. Zero capacities are valid. */
mulib_buildplan_t *mulib_buildplan_create(size_t targets,
                                           size_t dependencies);
/** Release owned names and tags. A destroy attempted from a callback is ignored. */
void mulib_buildplan_destroy(mulib_buildplan_t *plan);

/**
 * Find or insert a target. Repeated declarations merge by returning the same
 * stable ID. Names are limited to 4095 bytes and may not contain controls.
 */
int mulib_buildplan_target(mulib_buildplan_t *plan, const char *name,
                           size_t *out);
/** Look up a target without mutation. */
int mulib_buildplan_find(const mulib_buildplan_t *plan, const char *name,
                         size_t *out);
/** Add dependency -> dependent. Duplicate edges are idempotent. */
int mulib_buildplan_depend(mulib_buildplan_t *plan, size_t dependency,
                           size_t dependent);
/** Find or create both names and add dependency -> dependent. */
int mulib_buildplan_depend_names(mulib_buildplan_t *plan,
                                 const char *dependency,
                                 const char *dependent);

/** Replace the owned optional tag and the borrowed opaque action. */
int mulib_buildplan_set_tag(mulib_buildplan_t *plan, size_t id,
                            const char *tag);
int mulib_buildplan_set_action(mulib_buildplan_t *plan, size_t id,
                               const void *action);
/** Defaults are cpu=1, memory_mb=0, priority=0 and duration=0. */
int mulib_buildplan_set_resources(mulib_buildplan_t *plan, size_t id,
                                  unsigned cpu, unsigned memory_mb,
                                  int priority);
int mulib_buildplan_set_duration(mulib_buildplan_t *plan, size_t id,
                                 double duration);

size_t mulib_buildplan_count(const mulib_buildplan_t *plan);
size_t mulib_buildplan_dependency_count(const mulib_buildplan_t *plan);
const mulib_buildplan_target_t *
mulib_buildplan_at(const mulib_buildplan_t *plan, size_t id);
/** Borrow one dependency edge by insertion index. Outputs change on success. */
int mulib_buildplan_dependency_at(const mulib_buildplan_t *plan, size_t index,
                                  size_t *dependency, size_t *dependent);

/**
 * Produce a deterministic dependency-first order. out must hold count IDs;
 * it is unchanged on failure. Lexicographic target names break ready ties.
 */
int mulib_buildplan_order(const mulib_buildplan_t *plan, size_t *out,
                          size_t capacity);
/** Validate all IDs and acyclicity without running callbacks. */
int mulib_buildplan_validate(const mulib_buildplan_t *plan);

/**
 * Return one deterministic cycle as IDs with the first ID repeated at the end.
 * *length is set to the required size. NULL/short out returns FULL. If acyclic,
 * *length becomes zero and OK is returned.
 */
int mulib_buildplan_cycle(const mulib_buildplan_t *plan, size_t *out,
                          size_t capacity, size_t *length);

/**
 * Emit deterministic dependency waves that fit the CPU/memory budget.
 * A nonzero callback result is returned unchanged. The plan cannot be mutated
 * during callbacks.
 */
int mulib_buildplan_schedule(mulib_buildplan_t *plan,
                             mulib_buildplan_budget_t budget,
                             mulib_buildplan_wave_fn callback, void *user);

/**
 * Return the maximum-duration dependency path. The path is dependency-first.
 * *length and *total are set on success; a short path buffer returns FULL and
 * reports the required length. Durations are caller-supplied estimates.
 */
int mulib_buildplan_critical_path(const mulib_buildplan_t *plan, size_t *path,
                                  size_t capacity, size_t *length,
                                  double *total);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_BUILDPLAN) && !defined(MULIB_BUILDPLAN_IMPLEMENTED)
#define MULIB_BUILDPLAN_IMPLEMENTED

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct mulib_buildplan_edge_ {
  size_t dependency, dependent;
} mulib_buildplan_edge_;

struct mulib_buildplan {
  mulib_buildplan_target_t *targets;
  mulib_buildplan_edge_ *edges;
  size_t count, capacity, edge_count, edge_capacity;
  int walking;
};

static char *
mulib_buildplan_copy_(const char *text)
{
  size_t n;
  char *copy;
  if (!text)
    return NULL;
  n = strlen(text);
  if (n == SIZE_MAX)
    return NULL;
  copy = (char *)malloc(n + 1);
  if (copy)
    memcpy(copy, text, n + 1);
  return copy;
}

static int
mulib_buildplan_name_(const char *name)
{
  size_t i;
  if (!name || !*name)
    return 0;
  for (i = 0; name[i]; ++i) {
    unsigned char c = (unsigned char)name[i];
    if (i >= 4095 || c < 32 || c == 127)
      return 0;
  }
  return 1;
}

mulib_buildplan_t *
mulib_buildplan_create(size_t targets, size_t dependencies)
{
  mulib_buildplan_t *plan;
  if (targets > SIZE_MAX / sizeof(mulib_buildplan_target_t)
      || dependencies > SIZE_MAX / sizeof(mulib_buildplan_edge_))
    return NULL;
  plan = (mulib_buildplan_t *)calloc(1, sizeof(*plan));
  if (!plan)
    return NULL;
  if (targets)
    plan->targets = (mulib_buildplan_target_t *)calloc(
        targets, sizeof(*plan->targets));
  if (dependencies)
    plan->edges = (mulib_buildplan_edge_ *)malloc(
        dependencies * sizeof(*plan->edges));
  if ((targets && !plan->targets) || (dependencies && !plan->edges)) {
    mulib_buildplan_destroy(plan);
    return NULL;
  }
  plan->capacity = targets;
  plan->edge_capacity = dependencies;
  return plan;
}

void
mulib_buildplan_destroy(mulib_buildplan_t *plan)
{
  size_t i;
  if (!plan || plan->walking)
    return;
  for (i = 0; i < plan->count; ++i) {
    free((void *)plan->targets[i].name);
    free((void *)plan->targets[i].tag);
  }
  free(plan->targets);
  free(plan->edges);
  free(plan);
}

int
mulib_buildplan_find(const mulib_buildplan_t *plan, const char *name,
                     size_t *out)
{
  size_t i;
  if (!plan || !out || !mulib_buildplan_name_(name))
    return MULIB_BUILDPLAN_INVALID;
  for (i = 0; i < plan->count; ++i)
    if (!strcmp(plan->targets[i].name, name)) {
      *out = i;
      return MULIB_BUILDPLAN_OK;
    }
  return MULIB_BUILDPLAN_INVALID;
}

int
mulib_buildplan_target(mulib_buildplan_t *plan, const char *name, size_t *out)
{
  size_t i;
  char *copy;
  if (!plan || !out || !mulib_buildplan_name_(name))
    return MULIB_BUILDPLAN_INVALID;
  if (plan->walking)
    return MULIB_BUILDPLAN_BUSY;
  for (i = 0; i < plan->count; ++i)
    if (!strcmp(plan->targets[i].name, name)) {
      *out = i;
      return MULIB_BUILDPLAN_OK;
    }
  if (plan->count == plan->capacity)
    return MULIB_BUILDPLAN_FULL;
  copy = mulib_buildplan_copy_(name);
  if (!copy)
    return MULIB_BUILDPLAN_MEMORY;
  plan->targets[plan->count].name = copy;
  plan->targets[plan->count].cpu = 1;
  *out = plan->count++;
  return MULIB_BUILDPLAN_OK;
}

int
mulib_buildplan_depend(mulib_buildplan_t *plan, size_t dependency,
                       size_t dependent)
{
  size_t i;
  if (!plan || dependency >= plan->count || dependent >= plan->count)
    return MULIB_BUILDPLAN_INVALID;
  if (plan->walking)
    return MULIB_BUILDPLAN_BUSY;
  for (i = 0; i < plan->edge_count; ++i)
    if (plan->edges[i].dependency == dependency
        && plan->edges[i].dependent == dependent)
      return MULIB_BUILDPLAN_OK;
  if (plan->edge_count == plan->edge_capacity)
    return MULIB_BUILDPLAN_FULL;
  plan->edges[plan->edge_count].dependency = dependency;
  plan->edges[plan->edge_count++].dependent = dependent;
  return MULIB_BUILDPLAN_OK;
}

int
mulib_buildplan_depend_names(mulib_buildplan_t *plan, const char *dependency,
                             const char *dependent)
{
  size_t before, after, old_count;
  int rc;
  if (!plan)
    return MULIB_BUILDPLAN_INVALID;
  old_count = plan->count;
  rc = mulib_buildplan_target(plan, dependency, &before);
  if (rc)
    return rc;
  rc = mulib_buildplan_target(plan, dependent, &after);
  if (rc) {
    while (plan->count > old_count) {
      free((void *)plan->targets[plan->count - 1].name);
      memset(&plan->targets[--plan->count], 0, sizeof(*plan->targets));
    }
    return rc;
  }
  rc = mulib_buildplan_depend(plan, before, after);
  if (rc)
    while (plan->count > old_count) {
      free((void *)plan->targets[plan->count - 1].name);
      memset(&plan->targets[--plan->count], 0, sizeof(*plan->targets));
    }
  return rc;
}

int
mulib_buildplan_set_tag(mulib_buildplan_t *plan, size_t id, const char *tag)
{
  char *copy = NULL;
  if (!plan || id >= plan->count || (tag && !mulib_buildplan_name_(tag)))
    return MULIB_BUILDPLAN_INVALID;
  if (plan->walking)
    return MULIB_BUILDPLAN_BUSY;
  if (tag) {
    copy = mulib_buildplan_copy_(tag);
    if (!copy)
      return MULIB_BUILDPLAN_MEMORY;
  }
  free((void *)plan->targets[id].tag);
  plan->targets[id].tag = copy;
  return MULIB_BUILDPLAN_OK;
}

int
mulib_buildplan_set_action(mulib_buildplan_t *plan, size_t id,
                           const void *action)
{
  if (!plan || id >= plan->count)
    return MULIB_BUILDPLAN_INVALID;
  if (plan->walking)
    return MULIB_BUILDPLAN_BUSY;
  plan->targets[id].action = action;
  return MULIB_BUILDPLAN_OK;
}

int
mulib_buildplan_set_resources(mulib_buildplan_t *plan, size_t id,
                              unsigned cpu, unsigned memory_mb, int priority)
{
  if (!plan || id >= plan->count || !cpu)
    return MULIB_BUILDPLAN_INVALID;
  if (plan->walking)
    return MULIB_BUILDPLAN_BUSY;
  plan->targets[id].cpu = cpu;
  plan->targets[id].memory_mb = memory_mb;
  plan->targets[id].priority = priority;
  return MULIB_BUILDPLAN_OK;
}

int
mulib_buildplan_set_duration(mulib_buildplan_t *plan, size_t id,
                             double duration)
{
  if (!plan || id >= plan->count || !isfinite(duration) || duration < 0)
    return MULIB_BUILDPLAN_INVALID;
  if (plan->walking)
    return MULIB_BUILDPLAN_BUSY;
  plan->targets[id].duration = duration;
  return MULIB_BUILDPLAN_OK;
}

size_t
mulib_buildplan_count(const mulib_buildplan_t *plan)
{
  return plan ? plan->count : 0;
}

size_t
mulib_buildplan_dependency_count(const mulib_buildplan_t *plan)
{
  return plan ? plan->edge_count : 0;
}

const mulib_buildplan_target_t *
mulib_buildplan_at(const mulib_buildplan_t *plan, size_t id)
{
  return plan && id < plan->count ? plan->targets + id : NULL;
}

int
mulib_buildplan_dependency_at(const mulib_buildplan_t *plan, size_t index,
                              size_t *dependency, size_t *dependent)
{
  if (!plan || index >= plan->edge_count || (!dependency && !dependent))
    return MULIB_BUILDPLAN_INVALID;
  if (dependency)
    *dependency = plan->edges[index].dependency;
  if (dependent)
    *dependent = plan->edges[index].dependent;
  return MULIB_BUILDPLAN_OK;
}

static int
mulib_buildplan_order_(const mulib_buildplan_t *plan, size_t **result)
{
  size_t *order = NULL, *degree = NULL;
  unsigned char *done = NULL;
  size_t i, e, produced = 0;
  if (!plan || !result)
    return MULIB_BUILDPLAN_INVALID;
  *result = NULL;
  if (!plan->count)
    return MULIB_BUILDPLAN_OK;
  if (plan->count > SIZE_MAX / sizeof(*order))
    return MULIB_BUILDPLAN_OVERFLOW;
  order = (size_t *)malloc(plan->count * sizeof(*order));
  degree = (size_t *)calloc(plan->count, sizeof(*degree));
  done = (unsigned char *)calloc(plan->count, 1);
  if (!order || !degree || !done) {
    free(order);
    free(degree);
    free(done);
    return MULIB_BUILDPLAN_MEMORY;
  }
  for (e = 0; e < plan->edge_count; ++e) {
    if (plan->edges[e].dependency >= plan->count
        || plan->edges[e].dependent >= plan->count
        || degree[plan->edges[e].dependent] == SIZE_MAX) {
      free(order);
      free(degree);
      free(done);
      return MULIB_BUILDPLAN_INVALID;
    }
    ++degree[plan->edges[e].dependent];
  }
  while (produced < plan->count) {
    size_t best = SIZE_MAX;
    for (i = 0; i < plan->count; ++i)
      if (!done[i] && !degree[i]
          && (best == SIZE_MAX
              || strcmp(plan->targets[i].name,
                        plan->targets[best].name) < 0))
        best = i;
    if (best == SIZE_MAX) {
      free(order);
      free(degree);
      free(done);
      return MULIB_BUILDPLAN_CYCLE;
    }
    done[best] = 1;
    order[produced++] = best;
    for (e = 0; e < plan->edge_count; ++e)
      if (plan->edges[e].dependency == best)
        --degree[plan->edges[e].dependent];
  }
  free(degree);
  free(done);
  *result = order;
  return MULIB_BUILDPLAN_OK;
}

int
mulib_buildplan_order(const mulib_buildplan_t *plan, size_t *out,
                      size_t capacity)
{
  size_t *order;
  int rc;
  if (!plan || (plan->count && (!out || capacity < plan->count)))
    return MULIB_BUILDPLAN_INVALID;
  rc = mulib_buildplan_order_(plan, &order);
  if (rc)
    return rc;
  if (plan->count)
    memcpy(out, order, plan->count * sizeof(*out));
  free(order);
  return MULIB_BUILDPLAN_OK;
}

int
mulib_buildplan_validate(const mulib_buildplan_t *plan)
{
  size_t *order;
  int rc = mulib_buildplan_order_(plan, &order);
  free(order);
  return rc;
}

static size_t
mulib_buildplan_smallest_(const mulib_buildplan_t *plan,
                          const unsigned char *state)
{
  size_t i, best = SIZE_MAX;
  for (i = 0; i < plan->count; ++i)
    if (!state[i]
        && (best == SIZE_MAX
            || strcmp(plan->targets[i].name, plan->targets[best].name) < 0))
      best = i;
  return best;
}

int
mulib_buildplan_cycle(const mulib_buildplan_t *plan, size_t *out,
                      size_t capacity, size_t *length)
{
  unsigned char *state = NULL;
  size_t *stack = NULL, *edge_pos = NULL;
  size_t depth = 0, start, needed = 0, cycle_at = 0;
  int found = 0;
  if (!plan || !length)
    return MULIB_BUILDPLAN_INVALID;
  *length = 0;
  if (!plan->count)
    return MULIB_BUILDPLAN_OK;
  state = (unsigned char *)calloc(plan->count, 1);
  stack = (size_t *)malloc(plan->count * sizeof(*stack));
  edge_pos = (size_t *)calloc(plan->count, sizeof(*edge_pos));
  if (!state || !stack || !edge_pos) {
    free(state);
    free(stack);
    free(edge_pos);
    return MULIB_BUILDPLAN_MEMORY;
  }
  while (!found
         && (start = mulib_buildplan_smallest_(plan, state)) != SIZE_MAX) {
    stack[0] = start;
    edge_pos[0] = 0;
    state[start] = 1;
    depth = 1;
    while (depth && !found) {
      size_t node = stack[depth - 1], e;
      int advanced = 0;
      for (e = edge_pos[depth - 1]; e < plan->edge_count; ++e) {
        size_t next;
        edge_pos[depth - 1] = e + 1;
        if (plan->edges[e].dependency != node)
          continue;
        next = plan->edges[e].dependent;
        if (state[next] == 1) {
          for (cycle_at = 0; cycle_at < depth; ++cycle_at)
            if (stack[cycle_at] == next)
              break;
          needed = depth - cycle_at + 1;
          found = 1;
          break;
        }
        if (!state[next]) {
          state[next] = 1;
          stack[depth] = next;
          edge_pos[depth] = 0;
          ++depth;
          advanced = 1;
          break;
        }
      }
      if (!found && !advanced) {
        state[node] = 2;
        --depth;
      }
    }
  }
  if (found) {
    *length = needed;
    if (!out || capacity < needed) {
      free(state);
      free(stack);
      free(edge_pos);
      return MULIB_BUILDPLAN_FULL;
    }
    memcpy(out, stack + cycle_at, (needed - 1) * sizeof(*out));
    out[needed - 1] = stack[cycle_at];
  }
  free(state);
  free(stack);
  free(edge_pos);
  return MULIB_BUILDPLAN_OK;
}

static int
mulib_buildplan_ready_(const mulib_buildplan_t *plan,
                       const unsigned char *complete, size_t id)
{
  size_t e;
  if (complete[id])
    return 0;
  for (e = 0; e < plan->edge_count; ++e)
    if (plan->edges[e].dependent == id
        && !complete[plan->edges[e].dependency])
      return 0;
  return 1;
}

static int
mulib_buildplan_precedes_(const mulib_buildplan_t *plan, size_t a, size_t b)
{
  if (plan->targets[a].priority != plan->targets[b].priority)
    return plan->targets[a].priority > plan->targets[b].priority;
  return strcmp(plan->targets[a].name, plan->targets[b].name) < 0;
}

int
mulib_buildplan_schedule(mulib_buildplan_t *plan,
                         mulib_buildplan_budget_t budget,
                         mulib_buildplan_wave_fn callback, void *user)
{
  unsigned char *complete = NULL;
  size_t *ready = NULL, *wave = NULL;
  size_t completed = 0, i, j;
  int rc;
  if (!plan || !callback || !budget.cpu)
    return MULIB_BUILDPLAN_INVALID;
  if (plan->walking)
    return MULIB_BUILDPLAN_BUSY;
  rc = mulib_buildplan_validate(plan);
  if (rc)
    return rc;
  for (i = 0; i < plan->count; ++i)
    if (!plan->targets[i].cpu || plan->targets[i].cpu > budget.cpu
        || (budget.memory_mb
            && plan->targets[i].memory_mb > budget.memory_mb))
      return MULIB_BUILDPLAN_INVALID;
  complete = (unsigned char *)calloc(plan->count ? plan->count : 1, 1);
  ready = (size_t *)malloc((plan->count ? plan->count : 1) * sizeof(*ready));
  wave = (size_t *)malloc((plan->count ? plan->count : 1) * sizeof(*wave));
  if (!complete || !ready || !wave) {
    free(complete);
    free(ready);
    free(wave);
    return MULIB_BUILDPLAN_MEMORY;
  }
  plan->walking = 1;
  rc = MULIB_BUILDPLAN_OK;
  while (completed < plan->count && !rc) {
    size_t ready_count = 0, wave_count = 0;
    unsigned used_cpu = 0, used_memory = 0;
    for (i = 0; i < plan->count; ++i)
      if (mulib_buildplan_ready_(plan, complete, i))
        ready[ready_count++] = i;
    for (i = 1; i < ready_count; ++i) {
      size_t value = ready[i];
      j = i;
      while (j && mulib_buildplan_precedes_(plan, value, ready[j - 1])) {
        ready[j] = ready[j - 1];
        --j;
      }
      ready[j] = value;
    }
    for (i = 0; i < ready_count; ++i) {
      const mulib_buildplan_target_t *target = plan->targets + ready[i];
      if (target->cpu > budget.cpu - used_cpu)
        continue;
      if (budget.memory_mb
          && target->memory_mb > budget.memory_mb - used_memory)
        continue;
      wave[wave_count++] = ready[i];
      used_cpu += target->cpu;
      used_memory += target->memory_mb;
    }
    if (!wave_count) {
      rc = MULIB_BUILDPLAN_INVALID;
      break;
    }
    rc = callback(user, wave, wave_count);
    if (!rc)
      for (i = 0; i < wave_count; ++i) {
        complete[wave[i]] = 1;
        ++completed;
      }
  }
  plan->walking = 0;
  free(complete);
  free(ready);
  free(wave);
  return rc;
}

int
mulib_buildplan_critical_path(const mulib_buildplan_t *plan, size_t *path,
                              size_t capacity, size_t *length, double *total)
{
  size_t *order = NULL, *parent = NULL, *reverse = NULL;
  double *finish = NULL;
  size_t i, e, end = SIZE_MAX, needed = 0;
  int rc;
  if (!plan || !length || !total)
    return MULIB_BUILDPLAN_INVALID;
  *length = 0;
  *total = 0;
  rc = mulib_buildplan_order_(plan, &order);
  if (rc)
    return rc;
  if (!plan->count) {
    free(order);
    return MULIB_BUILDPLAN_OK;
  }
  parent = (size_t *)malloc(plan->count * sizeof(*parent));
  reverse = (size_t *)malloc(plan->count * sizeof(*reverse));
  finish = (double *)calloc(plan->count, sizeof(*finish));
  if (!parent || !reverse || !finish) {
    free(order);
    free(parent);
    free(reverse);
    free(finish);
    return MULIB_BUILDPLAN_MEMORY;
  }
  for (i = 0; i < plan->count; ++i)
    parent[i] = SIZE_MAX;
  for (i = 0; i < plan->count; ++i) {
    size_t id = order[i], best = SIZE_MAX;
    for (e = 0; e < plan->edge_count; ++e)
      if (plan->edges[e].dependent == id) {
        size_t candidate = plan->edges[e].dependency;
        if (best == SIZE_MAX || finish[candidate] > finish[best]
            || (finish[candidate] == finish[best]
                && strcmp(plan->targets[candidate].name,
                          plan->targets[best].name) < 0))
          best = candidate;
      }
    parent[id] = best;
    finish[id] = plan->targets[id].duration
                 + (best == SIZE_MAX ? 0 : finish[best]);
    if (!isfinite(finish[id])) {
      free(order);
      free(parent);
      free(reverse);
      free(finish);
      return MULIB_BUILDPLAN_OVERFLOW;
    }
    if (end == SIZE_MAX || finish[id] > finish[end]
        || (finish[id] == finish[end]
            && strcmp(plan->targets[id].name,
                      plan->targets[end].name) < 0))
      end = id;
  }
  *total = finish[end];
  while (end != SIZE_MAX) {
    reverse[needed++] = end;
    end = parent[end];
  }
  *length = needed;
  if (!path || capacity < needed)
    rc = MULIB_BUILDPLAN_FULL;
  else
    for (i = 0; i < needed; ++i)
      path[i] = reverse[needed - i - 1];
  free(order);
  free(parent);
  free(reverse);
  free(finish);
  return rc;
}

#endif
