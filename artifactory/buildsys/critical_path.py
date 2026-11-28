from .dep_resolve import topological_sort


def critical_path(graph, durations=None):
    source = graph.mapping() if hasattr(graph, "mapping") else graph
    durations = durations or {}
    if any(float(value) < 0 for value in durations.values()):
        raise ValueError("durations cannot be negative")
    finish = {}
    for n in topological_sort(source):
        finish[n] = durations.get(n, 0) + max(
            (finish[d] for d in source.get(n, ()) if d in finish), default=0
        )
    end = max(finish, key=finish.get, default=None)
    path = []
    while end is not None:
        path.append(end)
        preds = [
            d
            for d in source.get(end, ())
            if finish.get(d, 0) == finish[end] - durations.get(end, 0)
        ]
        end = max(preds, key=lambda x: finish[x], default=None)
    return list(reversed(path))
