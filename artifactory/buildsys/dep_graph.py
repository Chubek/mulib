from __future__ import annotations

from dataclasses import dataclass, field

from .target import Target


@dataclass
class DependencyGraph:
    nodes: dict[str, Target] = field(default_factory=dict)

    def add(self, target: Target) -> None:
        if not isinstance(target, Target):
            raise TypeError("target must be a Target")
        existing = self.nodes.get(target.name)
        if existing is None:
            self.nodes[target.name] = target
            return
        for dep in target.deps:
            if dep not in existing.deps:
                existing.deps.append(dep)
        for action in target.actions:
            if action not in existing.actions:
                existing.actions.append(action)
        for output in target.outputs:
            if output not in existing.outputs:
                existing.outputs.append(output)
        existing.metadata.update(target.metadata)

    def add_edge(self, dependency: str, dependent: str) -> None:
        if not dependency or not dependent:
            raise ValueError("dependency and dependent names must be non-empty")
        self.nodes.setdefault(dependent, Target(dependent))
        self.nodes.setdefault(dependency, Target(dependency))
        if dependency not in self.nodes[dependent].deps:
            self.nodes[dependent].deps.append(dependency)

    def dependencies(self, name: str) -> set[str]:
        out: set[str] = set()
        pending = list(self.nodes.get(name, Target(name)).deps)
        while pending:
            dependency = pending.pop()
            if dependency in out:
                continue
            out.add(dependency)
            pending.extend(self.nodes.get(dependency, Target(dependency)).deps)
        return out

    def dependents(self, name: str) -> set[str]:
        return {n for n in self.nodes if name in self.dependencies(n)}

    def mapping(self) -> dict[str, list[str]]:
        return {name: list(target.deps) for name, target in self.nodes.items()}

    def __contains__(self, name: str) -> bool:
        return name in self.nodes

    def __len__(self) -> int:
        return len(self.nodes)

    def __iter__(self):
        return iter(self.nodes.values())
