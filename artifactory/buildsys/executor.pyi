from subprocess import CompletedProcess

from .target import BuildAction

class ExecutionError(RuntimeError): ...

class Executor:
    check: bool
    def execute(self, action: BuildAction) -> CompletedProcess[str]: ...

class LocalExecutor(Executor): ...
