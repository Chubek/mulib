from __future__ import annotations

import json
import os
import re
import tempfile
from pathlib import Path
from typing import Any


class CacheError(ValueError):
    """Raised when a cache key or cache entry is invalid."""


class BuildCache:
    def __init__(self, root=".buildcache"):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        if not self.root.is_dir():
            raise CacheError(f"cache root is not a directory: {self.root}")

    @staticmethod
    def _validate_key(key: str) -> str:
        if not isinstance(key, str) or not re.fullmatch(r"[A-Za-z0-9._-]{1,256}", key):
            raise CacheError("cache keys must contain only letters, digits, '.', '_' or '-'")
        return key

    def _entry_path(self, key: str) -> Path:
        return self.root / f"{self._validate_key(key)}.json"

    def get(self, key: str) -> Any:
        path = self._entry_path(key)
        if not path.exists():
            return None
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise CacheError(f"invalid cache entry: {path}") from exc
        return value

    def put(self, key: str, value: Any) -> Any:
        path = self._entry_path(key)
        payload = json.dumps(
            value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")
        fd, temporary = tempfile.mkstemp(prefix=".cache-", suffix=".tmp", dir=self.root)
        try:
            with os.fdopen(fd, "wb") as stream:
                stream.write(payload)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, path)
        except OSError:
            Path(temporary).unlink(missing_ok=True)
            raise
        return value

    def invalidate(self, key: str) -> None:
        self._entry_path(key).unlink(missing_ok=True)

    def clear(self) -> None:
        for p in self.root.glob("*.json"):
            if p.is_file():
                p.unlink()


__all__ = ["BuildCache", "CacheError"]
