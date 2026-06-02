from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


@dataclass
class GraphNode:
    i: int
    ea: int
    name: str
    size: int


@dataclass
class CallGraph:
    meta: dict[str, Any]
    nodes: list[GraphNode]
    edges: list[tuple[int, int]]
    ea_to_i: dict[int, int] = field(default_factory=dict)

    @property
    def n(self) -> int:
        return len(self.nodes)


@dataclass
class ClusterConfig:
    algorithm: str = "scc"
    min_component_size: int = 1
    merge_enabled: bool = False
    merge_max_scc_size: int = 1
    merge_min_shared_edges: int = 1
    report_include_nodes: bool = True
    report_bucket_small: bool = False

    @classmethod
    def from_dict(cls, raw: dict[str, Any]) -> ClusterConfig:
        merge = raw.get("merge") or {}
        report = raw.get("report") or {}
        return cls(
            algorithm=str(raw.get("algorithm", "scc")).lower(),
            min_component_size=int(raw.get("min_component_size", 1)),
            merge_enabled=bool(merge.get("enabled", False)),
            merge_max_scc_size=int(merge.get("max_scc_size", 1)),
            merge_min_shared_edges=int(merge.get("min_shared_edges", 1)),
            report_include_nodes=bool(report.get("include_nodes", True)),
            report_bucket_small=bool(report.get("bucket_small", False)),
        )


@dataclass
class Cluster:
    id: int
    node_indices: list[int]

    @property
    def size(self) -> int:
        return len(self.node_indices)
