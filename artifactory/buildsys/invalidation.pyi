from enum import Enum

class InvalidationStrategy(Enum):
    TIMESTAMP: str
    CONTENT: str
    ALWAYS: str

def should_invalidate(strategy: InvalidationStrategy, old, new) -> bool: ...
