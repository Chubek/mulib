from dataclasses import dataclass


@dataclass
class Worker:
    name: str
    capacity: int = 1

    def __post_init__(self):
        if not self.name:
            raise ValueError("worker name must be non-empty")
        if self.capacity <= 0:
            raise ValueError("worker capacity must be positive")


class Coordinator:
    def __init__(self, workers=()):
        self.workers = list(workers)

    def choose(self):
        return max(self.workers, key=lambda w: (w.capacity, w.name), default=None)
