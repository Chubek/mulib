from __future__ import annotations

import json
from pathlib import Path

import pytest

from domainlet.bindlet_base import AnalysisContext, InputKind
from domainlet.bindlets.buildsys_bindlets import BuildSysBindlet
from domainlet.buildsys import BuildAction, DependencyGraph, Target
from domainlet.buildsys.cache import BuildCache, CacheError
from domainlet.buildsys.cmake import parse as parse_cmake
from domainlet.buildsys.dep_resolve import detect_cycles, topological_sort
from domainlet.buildsys.depfile import parse as parse_depfile
from domainlet.buildsys.graph_io import dumps, loads
from domainlet.buildsys.incremental import is_dirty
from domainlet.buildsys.manifest import ManifestError, validate
from domainlet.buildsys.resources import ResourceBudget, ResourceRequest, schedule
from domainlet.buildsys.sandbox import Sandbox


def test_graph_merges_duplicate_target_declarations() -> None:
    graph = DependencyGraph()
    graph.add(Target("app", ["lib"]))
    graph.add(Target("app", ["runtime"]))
    assert graph.mapping() == {"app": ["lib", "runtime"]}


def test_resolution_is_deterministic_and_cycle_safe() -> None:
    assert topological_sort({"app": ["lib"], "lib": []}) == ["lib", "app"]
    assert detect_cycles({"a": ["b"], "b": ["a"], "self": ["self"]}) == [
        ["a", "b", "a"],
        ["self", "self"],
    ]


def test_adapters_handle_realistic_syntax() -> None:
    assert parse_cmake("# add_executable(fake x)\nadd_executable(app main.c)").mapping() == {
        "app": ["main.c"]
    }
    assert parse_depfile("foo\\ bar.o: src\\ file.c include.h") == {
        "foo bar.o": ["src file.c", "include.h"]
    }


def test_graph_json_round_trip() -> None:
    graph = DependencyGraph()
    graph.add(Target("app", ["lib"], [BuildAction(["cc", "main.c"])]))
    graph.add(Target("lib"))
    assert loads(dumps(graph)).mapping() == graph.mapping()


def test_cache_rejects_traversal_and_writes_atomically(tmp_path: Path) -> None:
    cache = BuildCache(tmp_path / "cache")
    assert cache.put("key", {"value": 1}) == {"value": 1}
    assert cache.get("key") == {"value": 1}
    with pytest.raises(CacheError):
        cache.get("../outside")


def test_incremental_content_and_sandbox_guards(tmp_path: Path) -> None:
    source = tmp_path / "source"
    output = tmp_path / "output"
    source.write_text("same", encoding="utf-8")
    output.write_text("same", encoding="utf-8")
    assert not is_dirty([str(source)], [str(output)], content=True)
    sandbox = Sandbox(str(tmp_path))
    assert sandbox.path("nested/file").startswith(str(tmp_path))
    with pytest.raises(ValueError):
        sandbox.path("../outside")


def test_manifest_and_resource_scheduler() -> None:
    normalized = validate({"targets": {"app": {"deps": ["lib"]}, "lib": {}}})
    assert list(normalized["targets"]) == ["app", "lib"]
    with pytest.raises(ManifestError):
        validate({"targets": {"app": {"deps": ["missing"]}}})
    waves = schedule(
        {"app": ["lib"], "lib": [], "test": ["lib"]},
        {"app": ResourceRequest(cpu=2), "test": ResourceRequest(priority=1)},
        ResourceBudget(cpu=2),
    )
    assert waves == [["lib"], ["test"], ["app"]]


def test_bindlet_reports_structured_results() -> None:
    bindlet = BuildSysBindlet()
    bindlet.configure({"capability": "resolve"})
    issue = bindlet.execute(AnalysisContext(InputKind.SOURCE_TEXT, "app: lib\nlib:"))[0]
    assert issue.rule_id == "buildsys.order"
    assert json.loads(issue.extra["order"]) == ["lib", "app"]
