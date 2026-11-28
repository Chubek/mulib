import math
import time
from dataclasses import dataclass, field


@dataclass
class BuildProfile:
    records: list[dict] = field(default_factory=list)

    def record(self, name, duration, **extra):
        if not isinstance(name, str) or not name:
            raise ValueError("profile record name must be non-empty")
        duration = float(duration)
        if not math.isfinite(duration) or duration < 0:
            raise ValueError("profile duration must be finite and non-negative")
        self.records.append({"name": name, "duration": duration, **extra})

    def summary(self):
        return {
            "count": len(self.records),
            "total": sum(float(r["duration"]) for r in self.records),
            "slowest": max((float(r["duration"]) for r in self.records), default=0.0),
        }


class Timer:
    def __init__(self, profile, name):
        self.profile, self.name = profile, name

    def __enter__(self):
        self.start = time.perf_counter()
        return self

    def __exit__(self, *_):
        self.profile.record(self.name, time.perf_counter() - self.start)
