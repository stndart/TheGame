from __future__ import annotations

import gzip
import json
from pathlib import Path
from typing import Any

from textwall_graph.models import CallGraph, GraphNode


def _parse_ea(value: str | int) -> int:
    if isinstance(value, int):
        return value
    s = str(value).strip()
    if s.lower().startswith("0x"):
        return int(s, 16)
    return int(s, 0)


def load_graph(path: Path) -> CallGraph:
    """Load graph export (.json or .json.gz)."""
    raw: dict[str, Any]
    if path.suffix == ".gz" or str(path).endswith(".json.gz"):
        with gzip.open(path, "rt", encoding="utf-8") as f:
            raw = json.load(f)
    else:
        with path.open(encoding="utf-8") as f:
            raw = json.load(f)

    meta = raw.get("meta") or {}
    nodes_raw = raw["nodes"]
    edges_raw = raw["edges"]

    nodes: list[GraphNode] = []
    ea_to_i: dict[int, int] = {}
    for entry in nodes_raw:
        i = int(entry["i"])
        ea = _parse_ea(entry["ea"])
        name = str(entry.get("name") or "")
        size = int(entry.get("size") or 0)
        nodes.append(GraphNode(i=i, ea=ea, name=name, size=size))
        ea_to_i[ea] = i

    edges: list[tuple[int, int]] = []
    for pair in edges_raw:
        edges.append((int(pair[0]), int(pair[1])))

    return CallGraph(meta=meta, nodes=nodes, edges=edges, ea_to_i=ea_to_i)
