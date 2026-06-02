from __future__ import annotations

from collections import defaultdict


def merge_small_components(
    components: list[list[int]],
    edges: list[tuple[int, int]],
    max_size: int,
    min_shared_edges: int,
) -> tuple[list[list[int]], int]:
    """
    Merge components with size <= max_size into a neighbor with the most
    cross-edges on the component condensation graph.
    Returns (merged_components, merge_count).
    """
    if max_size < 1:
        return components, 0

    comp_lists = [list(c) for c in components]
    node_to_comp_map: dict[int, int] = {}
    for cid, comp in enumerate(comp_lists):
        for node in comp:
            node_to_comp_map[node] = cid

    n_comp = len(comp_lists)
    if n_comp == 0:
        return comp_lists, 0

    # Cross-edge weights between components (directed edges, both directions counted)
    cross: dict[tuple[int, int], int] = defaultdict(int)
    for u, v in edges:
        cu = node_to_comp_map.get(u)
        cv = node_to_comp_map.get(v)
        if cu is None or cv is None or cu == cv:
            continue
        cross[(cu, cv)] += 1

    parent = list(range(n_comp))

    def find(x: int) -> int:
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a: int, b: int) -> None:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    merge_count = 0
    # Process small components smallest-first
    order = sorted(range(n_comp), key=lambda i: len(comp_lists[i]))
    for cid in order:
        if find(cid) != cid:
            continue
        size = len(comp_lists[cid])
        if size > max_size:
            continue

        # Neighbor weights: sum cross edges to other live components
        weights: dict[int, int] = defaultdict(int)
        for (a, b), w in cross.items():
            ra, rb = find(a), find(b)
            if ra == rb:
                continue
            if ra == cid:
                weights[rb] += w
            elif rb == cid:
                weights[ra] += w

        if not weights:
            continue

        best_neighbor = max(weights, key=lambda k: weights[k])
        if weights[best_neighbor] < min_shared_edges:
            continue

        union(cid, best_neighbor)
        merge_count += 1

    # Materialize merged components
    merged_buckets: dict[int, list[int]] = defaultdict(list)
    for cid, comp in enumerate(comp_lists):
        root = find(cid)
        merged_buckets[root].extend(comp)

    return list(merged_buckets.values()), merge_count
