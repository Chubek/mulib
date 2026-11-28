from __future__ import annotations

from collections.abc import Mapping


def predict(records, features=None):
    values = list(records)
    vals = [item.get("duration", 0) if isinstance(item, Mapping) else item for item in values]
    if any(not isinstance(value, (int, float)) or value < 0 for value in vals):
        raise ValueError("build durations must be non-negative numbers")
    return sum(vals) / len(vals) if vals else 0.0
