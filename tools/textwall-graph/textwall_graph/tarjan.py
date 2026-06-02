from __future__ import annotations


def strongly_connected_components(n: int, edges: list[tuple[int, int]]) -> list[list[int]]:
    """Tarjan SCC on directed graph with n nodes (0..n-1)."""
    adj: list[list[int]] = [[] for _ in range(n)]
    for u, v in edges:
        if 0 <= u < n and 0 <= v < n:
            adj[u].append(v)

    index = 0
    stack: list[int] = []
    on_stack = [False] * n
    indices = [-1] * n
    lowlink = [-1] * n
    result: list[list[int]] = []

    def strongconnect(v: int) -> None:
        nonlocal index
        indices[v] = index
        lowlink[v] = index
        index += 1
        stack.append(v)
        on_stack[v] = True

        for w in adj[v]:
            if indices[w] == -1:
                strongconnect(w)
                lowlink[v] = min(lowlink[v], lowlink[w])
            elif on_stack[w]:
                lowlink[v] = min(lowlink[v], indices[w])

        if lowlink[v] == indices[v]:
            component: list[int] = []
            while True:
                w = stack.pop()
                on_stack[w] = False
                component.append(w)
                if w == v:
                    break
            result.append(component)

    for v in range(n):
        if indices[v] == -1:
            strongconnect(v)

    return result
