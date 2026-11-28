from dataclasses import dataclass, field


@dataclass
class GeneratedFile:
    path: str
    source: str = ""
    command: str | None = None

    def __post_init__(self):
        if not self.path:
            raise ValueError("generated file path must be non-empty")


@dataclass
class CodegenTracker:
    files: list[GeneratedFile] = field(default_factory=list)

    def add(self, path, source="", command=None):
        generated = GeneratedFile(path, source, command)
        self.files = [item for item in self.files if item.path != generated.path]
        self.files.append(generated)
