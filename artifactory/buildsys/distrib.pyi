class Worker:
    name: str
    capacity: int

class Coordinator:
    workers: list[Worker]
    def choose(self) -> Worker | None: ...
