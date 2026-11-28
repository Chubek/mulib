from __future__ import annotations

from enum import Enum


class InvalidationStrategy(Enum):
    TIMESTAMP = "timestamp"
    CONTENT = "content"
    ALWAYS = "always"


def should_invalidate(strategy, old, new):
    if isinstance(strategy, str):
        try:
            strategy = InvalidationStrategy(strategy.lower())
        except ValueError as exc:
            raise ValueError(f"unknown invalidation strategy: {strategy}") from exc
    if not isinstance(strategy, InvalidationStrategy):
        raise TypeError("strategy must be an InvalidationStrategy")
    return strategy is InvalidationStrategy.ALWAYS or old != new
