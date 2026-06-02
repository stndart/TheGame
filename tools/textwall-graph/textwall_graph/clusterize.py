from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path
from typing import Any

import yaml

from textwall_graph.load import load_graph
from textwall_graph.merge import merge_small_components
from textwall_graph.models import CallGraph, Cluster, ClusterConfig
from textwall_graph.tarjan import strongly_connected_components
from textwall_graph.wcc import weakly_connected_components


def load_config(path: Path) -> ClusterConfig:
    with path.open(encoding="utf-8") as f:
        raw = yaml.safe_load(f) or {}
    return ClusterConfig.from_dict(raw)


def partition(graph: CallGraph, cfg: ClusterConfig) -> tuple[list[list[int]], int]:
    n = graph.n
    if cfg.algorithm == "wcc":
        components = weakly_connected_components(n, graph.edges)
        merged_from = 0
    else:
        components = strongly_connected_components(n, graph.edges)
        merged_from = 0

    if cfg.merge_enabled and cfg.algorithm == "scc":
        components, merged_from = merge_small_components(
            components,
            graph.edges,
            cfg.merge_max_scc_size,
            cfg.merge_min_shared_edges,
        )

    return components, merged_from


def filter_components(
    components: list[list[int]], cfg: ClusterConfig
) -> tuple[list[list[int]], list[int]]:
    """Apply min_component_size; optionally bucket small into cluster id -1."""
    kept: list[list[int]] = []
    bucket: list[int] = []
    for comp in components:
        if len(comp) >= cfg.min_component_size:
            kept.append(comp)
        elif cfg.report_bucket_small:
            bucket.extend(comp)
        # else drop from report

    if bucket:
        kept.append(bucket)
    return kept, bucket


def build_clusters(graph: CallGraph, components: list[list[int]]) -> list[Cluster]:
    clusters: list[Cluster] = []
    for cid, nodes in enumerate(
        sorted(components, key=lambda c: (-len(c), min(c) if c else 0))
    ):
        clusters.append(Cluster(id=cid, node_indices=sorted(nodes)))
    return clusters


def cluster_stats(components: list[list[int]]) -> dict[str, Any]:
    sizes = [len(c) for c in components]
    if not sizes:
        return {"components": 0, "largest": 0, "median": 0}
    sizes_sorted = sorted(sizes)
    mid = len(sizes_sorted) // 2
    median = sizes_sorted[mid] if len(sizes_sorted) % 2 else (
        sizes_sorted[mid - 1] + sizes_sorted[mid]
    ) / 2
    return {
        "components": len(sizes),
        "largest": max(sizes),
        "median": median,
        "singletons": sum(1 for s in sizes if s == 1),
    }


def emit_outputs(
    graph: CallGraph,
    cfg: ClusterConfig,
    config_raw: dict[str, Any],
    clusters: list[Cluster],
    merged_from: int,
    out_json: Path,
    out_tsv: Path,
) -> None:
    image_base = int(str(graph.meta.get("image_base", "0x400000")), 0)

    cluster_rows: list[dict[str, Any]] = []
    for cl in clusters:
        eas = [graph.nodes[i].ea for i in cl.node_indices]
        rva_min = min(eas) - image_base
        rva_max = max(eas) - image_base
        row: dict[str, Any] = {
            "id": cl.id,
            "size": cl.size,
            "rva_min": f"0x{rva_min:X}",
            "rva_max": f"0x{rva_max:X}",
        }
        if cfg.report_include_nodes:
            row["nodes"] = cl.node_indices
        cluster_rows.append(row)

    payload = {
        "config": config_raw,
        "stats": {
            **cluster_stats([c.node_indices for c in clusters]),
            "merged_from": merged_from,
        },
        "clusters": cluster_rows,
    }

    out_json.parent.mkdir(parents=True, exist_ok=True)
    with out_json.open("w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)

    with out_tsv.open("w", encoding="utf-8") as f:
        f.write("cluster_id\trva\tea\tname\n")
        for cl in clusters:
            for i in cl.node_indices:
                node = graph.nodes[i]
                rva = node.ea - image_base
                name = node.name.replace("\t", " ").replace("\n", " ")
                f.write(f"{cl.id}\t0x{rva:X}\t0x{node.ea:X}\t{name}\n")


def print_histogram(clusters: list[Cluster]) -> None:
    buckets = Counter()
    for cl in clusters:
        s = cl.size
        if s == 1:
            buckets["1"] += 1
        elif s <= 5:
            buckets["2-5"] += 1
        elif s <= 20:
            buckets["6-20"] += 1
        elif s <= 100:
            buckets["21-100"] += 1
        elif s <= 500:
            buckets["101-500"] += 1
        else:
            buckets["500+"] += 1
    print("Size histogram (components per bucket):")
    for key in ("1", "2-5", "6-20", "21-100", "101-500", "500+"):
        if key in buckets:
            print(f"  {key}: {buckets[key]}")


def run(
    graph_path: Path,
    config_path: Path,
    out_json: Path,
    out_tsv: Path,
    histogram: bool,
) -> int:
    with config_path.open(encoding="utf-8") as f:
        config_raw = yaml.safe_load(f) or {}

    cfg = ClusterConfig.from_dict(config_raw)
    graph = load_graph(graph_path)

    components, merged_from = partition(graph, cfg)
    components, _bucket = filter_components(components, cfg)
    clusters = build_clusters(graph, components)

    emit_outputs(graph, cfg, config_raw, clusters, merged_from, out_json, out_tsv)

    stats = cluster_stats([c.node_indices for c in clusters])
    print(
        f"Loaded {graph.n} nodes, {len(graph.edges)} edges from {graph_path.name}"
    )
    print(
        f"Algorithm={cfg.algorithm} merge={cfg.merge_enabled} -> "
        f"{stats['components']} components, largest={stats['largest']}, "
        f"merged_from={merged_from}"
    )
    print(f"Wrote {out_json}")
    print(f"Wrote {out_tsv}")

    if histogram:
        print_histogram(clusters)

    return 0


def main(argv: list[str] | None = None) -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description="Cluster .text call graph (SCC / WCC + optional merge)"
    )
    parser.add_argument(
        "graph",
        type=Path,
        help="graph.json or graph.json.gz from IDA export",
    )
    parser.add_argument(
        "-c",
        "--config",
        type=Path,
        default=root / "config" / "default.yaml",
        help="YAML config (algorithm, merge thresholds)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=root / "out" / "clusters.json",
        help="Output clusters JSON",
    )
    parser.add_argument(
        "--tsv",
        type=Path,
        default=None,
        help="Output TSV (default: same stem as -o with .tsv)",
    )
    parser.add_argument(
        "--histogram",
        action="store_true",
        help="Print component size histogram",
    )
    args = parser.parse_args(argv)

    out_tsv = args.tsv
    if out_tsv is None:
        out_tsv = args.output.with_suffix(".tsv")

    if not args.graph.is_file():
        print(f"Graph not found: {args.graph}", file=sys.stderr)
        return 1
    if not args.config.is_file():
        print(f"Config not found: {args.config}", file=sys.stderr)
        return 1

    return run(args.graph, args.config, args.output, out_tsv, args.histogram)


if __name__ == "__main__":
    raise SystemExit(main())
