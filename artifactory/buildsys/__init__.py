"""Build-system domainlets.

The modules in this package intentionally use small, composable Python data
models so they can be used by CMake/Ninja/Make adapters as well as directly
from Python.
"""

from .artifact import Artifact
from .dep_graph import DependencyGraph
from .dep_resolve import detect_cycles, topological_sort
from .depfile import DepfileError, parse_depfile
from .graph_io import dumps as graph_dumps
from .graph_io import loads as graph_loads
from .graph_io import to_dict, to_dot
from .manifest import ManifestError, validate
from .manifest import dump as dump_manifest
from .manifest import load as load_manifest
from .provenance import ActionProvenance
from .resources import ResourceBudget, ResourceRequest, ResourceScheduler
from .target import BuildAction, Target

__all__ = [
    "ActionProvenance",
    "Artifact",
    "BuildAction",
    "DependencyGraph",
    "DepfileError",
    "ManifestError",
    "ResourceBudget",
    "ResourceRequest",
    "ResourceScheduler",
    "Target",
    "detect_cycles",
    "dump_manifest",
    "graph_dumps",
    "graph_loads",
    "load_manifest",
    "parse_depfile",
    "to_dict",
    "to_dot",
    "topological_sort",
    "validate",
]
