from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Sandbox:
    root: str
    readonly: list[str] = field(default_factory=list)
    writable: list[str] = field(default_factory=list)

    def __post_init__(self):
        if not self.root:
            raise ValueError("sandbox root must be non-empty")
        self._root_path = Path(self.root).resolve()
        self.readonly = [str(Path(path)) for path in self.readonly]
        self.writable = [str(Path(path)) for path in self.writable]

    def path(self, name: str) -> str:
        candidate = (self._root_path / name).resolve()
        try:
            candidate.relative_to(self._root_path)
        except ValueError as exc:
            raise ValueError(f"path escapes sandbox root: {name!r}") from exc
        return str(candidate)
