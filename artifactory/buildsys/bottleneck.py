def find_bottlenecks(records, threshold=0.9):
    if not 0 <= threshold <= 1:
        raise ValueError("threshold must be between 0 and 1")
    records = list(records)
    vals = [r.get("duration", 0) if isinstance(r, dict) else r for r in records]
    if any(not isinstance(value, (int, float)) or value < 0 for value in vals):
        raise ValueError("durations must be non-negative numbers")
    if not vals:
        return []
    cutoff = sorted(vals)[int((len(vals) - 1) * threshold)]
    return [r for r, v in zip(records, vals) if v >= cutoff]
