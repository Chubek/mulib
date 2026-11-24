/** @file mulib-isched.h
 * @brief Bounded single-issue DAG scheduling with latency and shared resources.
 * Define MULIB_IMPL_ISCHED. No dependencies.
 */
#ifndef MULIB_ISCHED_H
#define MULIB_ISCHED_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_isched_node
  {
    size_t latency;
  } mulib_isched_node_t;
  typedef struct mulib_isched_edge
  {
    size_t before, after;
  } mulib_isched_edge_t;
  typedef struct mulib_isched_limits
  {
    size_t nodes, edges, cycles;
  } mulib_isched_limits_t;
  typedef enum mulib_isched_status
  {
    MULIB_ISCHED_OK,
    MULIB_ISCHED_INVALID,
    MULIB_ISCHED_CYCLE,
    MULIB_ISCHED_RESOURCE_LIMIT,
    MULIB_ISCHED_OVERFLOW,
    MULIB_ISCHED_NO_MEMORY
  } mulib_isched_status_t;
  /** Owned arrays: order[issue position], start[node ID]. Zero initialize. */
  typedef struct mulib_isched_schedule
  {
    size_t *order, *start;
    size_t count, makespan;
  } mulib_isched_schedule_t;
  typedef struct mulib_isched_diagnostic
  {
    mulib_isched_status_t status;
    size_t node;
  } mulib_isched_diagnostic_t;
  /** Optional issue-resource annotations, e.g. translated from ArchML. Each bit
   * identifies a shared unit-capacity resource. throughput is its occupancy in
   * cycles (positive). A zero mask uses only the single global issue slot. */
  typedef struct mulib_isched_resources
  {
    uint64_t mask;
    size_t throughput;
  } mulib_isched_resources_t;
  /** Prioritize longest remaining dependency path; ties use lowest node ID.
   * Edges mean start[after] >= start[before]+latency[before]. One issue per
   * cycle; operations may overlap. Positive latencies required. Empty DAG is
   * valid. Failure preserves empty output. Duplicate edges are allowed.
   */
  mulib_isched_status_t
  mulib_isched_run (const mulib_isched_node_t *nodes, size_t count,
                    const mulib_isched_edge_t *edges, size_t edge_count,
                    mulib_isched_limits_t limits,
                    mulib_isched_schedule_t *output,
                     mulib_isched_diagnostic_t *diagnostic);
  /** Schedule the DAG, then delay issue as needed for shared resources. This
   * conservative list policy retains critical-path order rather than searching
   * for a different order during a resource stall. preserve_order selects input
   * order instead; it requires all edges to point forward. Transactional output.
   * Resource capacities greater than one can be conservatively modeled as one. */
  mulib_isched_status_t mulib_isched_run_resources (
      const mulib_isched_node_t *, const mulib_isched_resources_t *, size_t count,
      const mulib_isched_edge_t *, size_t edge_count, int preserve_order,
      mulib_isched_limits_t, mulib_isched_schedule_t *, mulib_isched_diagnostic_t *);
  /** Release arrays, zero schedule. */
  void mulib_isched_free (mulib_isched_schedule_t *schedule);
  /** Static status description. */
  const char *mulib_isched_status_string (mulib_isched_status_t status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ISCHED) && !defined(MULIB_ISCHED_IMPLEMENTED)
#define MULIB_ISCHED_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_isched_free (mulib_isched_schedule_t *s)
  {
    if (s)
      {
        free (s->order);
        free (s->start);
        memset (s, 0, sizeof (*s));
      }
  }
  mulib_isched_status_t
  mulib_isched_run (const mulib_isched_node_t *nodes, size_t n,
                    const mulib_isched_edge_t *edges, size_t m,
                    mulib_isched_limits_t limits, mulib_isched_schedule_t *out,
                    mulib_isched_diagnostic_t *d)
  {
    size_t *degree = NULL, *height = NULL, *topo = NULL, *ready = NULL;
    unsigned char *done = NULL;
    mulib_isched_schedule_t r = { NULL, NULL, 0, 0 };
    mulib_isched_status_t s = MULIB_ISCHED_OK;
    size_t k = 0, j, pos, time = 0;
    if (!out || out->order || out->start || out->count || out->makespan || (!nodes && n)
        || (!edges && m))
      {
        s = MULIB_ISCHED_INVALID;
        goto finish;
      }
    if (n > limits.nodes || m > limits.edges || n > SIZE_MAX / sizeof (size_t)
        || m > SIZE_MAX / sizeof (*edges))
      {
        s = MULIB_ISCHED_RESOURCE_LIMIT;
        goto finish;
      }
    for (k = 0; k < n; ++k)
      if (!nodes[k].latency)
        {
          s = MULIB_ISCHED_INVALID;
          goto finish;
        }
    for (j = 0; j < m; ++j)
      if (edges[j].before >= n || edges[j].after >= n)
        {
          k = j;
          s = MULIB_ISCHED_INVALID;
          goto finish;
        }
    if (!n)
      goto success;
    degree = (size_t *)calloc (n, sizeof (*degree));
    height = (size_t *)calloc (n, sizeof (*height));
    topo = (size_t *)malloc (n * sizeof (*topo));
    ready = (size_t *)calloc (n, sizeof (*ready));
    done = (unsigned char *)calloc (n, 1);
    r.order = (size_t *)malloc (n * sizeof (*r.order));
    r.start = (size_t *)malloc (n * sizeof (*r.start));
    if (!degree || !height || !topo || !ready || !done || !r.order || !r.start)
      {
        s = MULIB_ISCHED_NO_MEMORY;
        goto finish;
      }
    for (j = 0; j < m; ++j)
      ++degree[edges[j].after];
    for (pos = 0; pos < n; ++pos)
      {
        for (k = 0; k < n && (done[k] || degree[k]); ++k)
          {
          }
        if (k == n)
          {
            for (k = 0; k < n && done[k]; ++k)
              {
              }
            s = MULIB_ISCHED_CYCLE;
            goto finish;
          }
        topo[pos] = k;
        done[k] = 1;
        for (j = 0; j < m; ++j)
          if (edges[j].before == k)
            --degree[edges[j].after];
      }
    for (pos = n; pos-- > 0;)
      {
        size_t tail = 0;
        k = topo[pos];
        for (j = 0; j < m; ++j)
          if (edges[j].before == k && height[edges[j].after] > tail)
            tail = height[edges[j].after];
        if (nodes[k].latency > SIZE_MAX - tail)
          {
            s = MULIB_ISCHED_OVERFLOW;
            goto finish;
          }
        height[k] = nodes[k].latency + tail;
      }
    memset (done, 0, n);
    for (j = 0; j < m; ++j)
      ++degree[edges[j].after];
    for (pos = 0; pos < n; ++pos)
      {
        size_t best = SIZE_MAX, earliest = SIZE_MAX, end;
        for (k = 0; k < n; ++k)
          if (!done[k] && !degree[k])
            {
              if (ready[k] < earliest)
                earliest = ready[k];
              if (ready[k] <= time
                  && (best == SIZE_MAX || height[k] > height[best]))
                best = k;
            }
        if (best == SIZE_MAX)
          {
            time = earliest;
            for (k = 0; k < n; ++k)
              if (!done[k] && !degree[k] && ready[k] <= time
                  && (best == SIZE_MAX || height[k] > height[best]))
                best = k;
          }
        k = best;
        if (nodes[k].latency > SIZE_MAX - time)
          {
            s = MULIB_ISCHED_OVERFLOW;
            goto finish;
          }
        end = time + nodes[k].latency;
        if (end > limits.cycles)
          {
            s = MULIB_ISCHED_RESOURCE_LIMIT;
            goto finish;
          }
        r.order[pos] = k;
        r.start[k] = time;
        done[k] = 1;
        if (end > r.makespan)
          r.makespan = end;
        for (j = 0; j < m; ++j)
          if (edges[j].before == k)
            {
              size_t a = edges[j].after;
              --degree[a];
              if (ready[a] < end)
                ready[a] = end;
            }
        if (pos + 1 < n)
          {
            if (time == SIZE_MAX)
              {
                s = MULIB_ISCHED_OVERFLOW;
                goto finish;
              }
            ++time;
          }
      }
  success:
    r.count = n;
    *out = r;
    memset (&r, 0, sizeof (r));
  finish:
    free (degree);
    free (height);
    free (topo);
    free (ready);
    free (done);
    mulib_isched_free (&r);
    if (d)
      {
        d->status = s;
        d->node = k;
      }
    return s;
  }
  const char *
  mulib_isched_status_string (mulib_isched_status_t s)
  {
    switch (s)
      {
      case MULIB_ISCHED_OK:
        return "ok";
      case MULIB_ISCHED_INVALID:
        return "invalid graph, latency, or output";
      case MULIB_ISCHED_CYCLE:
        return "dependency cycle";
      case MULIB_ISCHED_RESOURCE_LIMIT:
        return "resource limit";
      case MULIB_ISCHED_OVERFLOW:
        return "cycle arithmetic overflow";
      case MULIB_ISCHED_NO_MEMORY:
        return "allocation failed";
      default:
        return "unknown scheduler status";
      }
  }
  mulib_isched_status_t mulib_isched_run_resources (
      const mulib_isched_node_t *nodes, const mulib_isched_resources_t *resources,
      size_t n, const mulib_isched_edge_t *edges, size_t m, int preserve_order,
      mulib_isched_limits_t limits, mulib_isched_schedule_t *out,
      mulib_isched_diagnostic_t *d)
  {
    mulib_isched_schedule_t r = { NULL, NULL, 0, 0 };
    mulib_isched_status_t s = MULIB_ISCHED_INVALID;
    size_t ready[64] = {0}, time = 0, k = 0;
    if (!out || out->order || out->start || out->count || out->makespan
        || (!nodes && n) || (!resources && n) || (!edges && m)) goto done;
    if (n > limits.nodes || m > limits.edges
        || n > SIZE_MAX / sizeof (*resources) || n > SIZE_MAX / sizeof (*nodes)
        || m > SIZE_MAX / sizeof (*edges)) { s = MULIB_ISCHED_RESOURCE_LIMIT; goto done; }
    for (k = 0; k < n; ++k) if (!resources[k].throughput || !nodes[k].latency) goto done;
    if (preserve_order) {
      for (k = 0; k < m; ++k)
        if (edges[k].before >= edges[k].after || edges[k].after >= n) goto done;
      if (n) {
        r.order = (size_t *)malloc (n * sizeof (*r.order));
        r.start = (size_t *)malloc (n * sizeof (*r.start));
        if (!r.order || !r.start) { s = MULIB_ISCHED_NO_MEMORY; goto done; }
      }
      r.count = n;
    } else {
      s = mulib_isched_run (nodes, n, edges, m, limits, &r, d);
      if (s != MULIB_ISCHED_OK) return s;
    }
    r.makespan = 0;
    for (size_t j = 0; j < n; ++j) {
      size_t end;
      k = preserve_order ? j : r.order[j];
      for (size_t e = 0; e < m; ++e) if (edges[e].after == k) {
        size_t before = edges[e].before, finish = r.start[before] + nodes[before].latency;
        if (finish > time) time = finish;
      }
      for (size_t q = 0; q < 64; ++q)
        if ((resources[k].mask & (UINT64_C(1) << q)) && ready[q] > time) time = ready[q];
      s = MULIB_ISCHED_OVERFLOW;
      if (nodes[k].latency > SIZE_MAX - time
          || (resources[k].mask && resources[k].throughput > SIZE_MAX - time)) goto done;
      end = time + nodes[k].latency;
      s = MULIB_ISCHED_RESOURCE_LIMIT; if (end > limits.cycles) goto done;
      r.start[k] = time; r.order[j] = k;
      for (size_t q = 0; q < 64; ++q)
        if (resources[k].mask & (UINT64_C(1) << q)) ready[q] = time + resources[k].throughput;
      if (end > r.makespan) r.makespan = end;
      if (j + 1 < n) {
        if (time == SIZE_MAX) { s = MULIB_ISCHED_OVERFLOW; goto done; }
        ++time;
      }
    }
    *out = r; memset (&r, 0, sizeof (r)); s = MULIB_ISCHED_OK;
  done:
    mulib_isched_free (&r);
    if (d) { d->status = s; d->node = k; }
    return s;
  }
#ifdef __cplusplus
}
#endif
#endif
