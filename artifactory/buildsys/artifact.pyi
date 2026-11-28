class Artifact:
    path: str
    fingerprint: str | None
    size: int
    mtime: float
    kind: str
    @classmethod
    def from_path(cls, path: str, content: bool = ...) -> Artifact: ...
    def exists(self) -> bool: ...
