class GeneratedFile:
    path: str
    source: str
    command: str | None

class CodegenTracker:
    files: list[GeneratedFile]
    def add(self, path: str, source: str = ..., command=...) -> None: ...
