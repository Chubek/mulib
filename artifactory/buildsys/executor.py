from __future__ import annotations

from dataclasses import dataclass
from subprocess import CompletedProcess

from .core import BuildAction, BuildError, run_action


class ExecutionError(RuntimeError):
    """Raised when an action cannot be started or violates execution policy."""

    def __init__(self, message: str, action: BuildAction | None = None):
        super().__init__(message)
        self.action = action


@dataclass
class Executor:
    check: bool = False

    def execute(self, action: BuildAction) -> CompletedProcess[str]:
        try:
            result = run_action(action)
        except (OSError, BuildError) as exc:
            raise ExecutionError(str(exc), action) from exc
        if self.check and result.returncode != 0:
            raise ExecutionError(
                f"action failed with exit status {result.returncode}: {action.argv()}",
                action,
            )
        return result


class LocalExecutor(Executor):
    """Explicit local executor retained as a stable extension point."""

    def execute(self, action: BuildAction) -> CompletedProcess[str]:
        return super().execute(action)


__all__ = ["ExecutionError", "Executor", "LocalExecutor"]
