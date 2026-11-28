from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class BuildConfig:
    values: dict[str, str] = field(default_factory=dict)

    def __post_init__(self):
        self.values = {str(key): str(value) for key, value in self.values.items()}

    def get(self, key, default=None):
        return self.values.get(key, default)

    def set(self, key, value):
        if not str(key):
            raise ValueError("configuration keys must be non-empty")
        self.values[str(key)] = str(value)

    @classmethod
    def from_mapping(cls, mapping):
        return cls({str(k): str(v) for k, v in mapping.items()})
