"""
Export executable code-segment call graph for textwall-graph clustering.

Run inside IDA with GAME.idb open:
  File -> Script file...  (or IDA MCP py_exec_file)

Environment:
  TEXTWALL_OUT                 output path (default: <repo>/tools/textwall-graph/out/graph.json.gz)
  TEXTWALL_SEGMENT             segment selector (default: auto)
                                 auto   — all IDA class CODE segments (GAME.exe: two unnamed)
                                 .text  — single segment by name if present
                                 seg1,seg2 — comma-separated names; empty name = seg_<start_ea>
  TEXTWALL_LIST_SEGMENTS=1     print segments and exit (no export)
  TEXTWALL_INCLUDE_LIB_THUNKS  1 to include library thunk stubs

Stdout prints summary and output path.
"""
from __future__ import print_function

import gzip
import json
import os

import ida_funcs
import ida_segment
import idautils
import idc

EXPORT_VERSION = 2

# IDA segment class for executable code
_SEG_CLASS_CODE = "CODE"


def _env(name, default=None):
    v = os.environ.get(name)
    return v if v is not None and v != "" else default


def _default_out_path():
    try:
        here = os.path.dirname(os.path.abspath(__file__))
        root = os.path.dirname(here)
        return os.path.join(root, "out", "graph.json.gz")
    except Exception:
        return os.path.join(os.getcwd(), "graph.json.gz")


def _seg_display_name(seg):
    """Stable label for export/meta; unnamed PE sections often return ''."""
    name = ida_segment.get_segm_name(seg) or ""
    name = name.strip()
    if name:
        return name
    return "seg_%08X" % seg.start_ea


def _seg_info_dict(seg):
    return {
        "name": _seg_display_name(seg),
        "start": "0x%X" % seg.start_ea,
        "end": "0x%X" % seg.end_ea,
        "size": int(seg.end_ea - seg.start_ea),
        "class": ida_segment.get_segm_class(seg) or "",
    }


def _iter_all_segments():
    for i in range(ida_segment.get_segm_qty()):
        seg = ida_segment.getnseg(i)
        if seg:
            yield seg


def _is_code_segment(seg):
    if ida_segment.get_segm_class(seg) == _SEG_CLASS_CODE:
        return True
    # Fallback: executable permission without insisting on class string
    perm = getattr(seg, "perm", 0)
    if perm & ida_segment.SEGPERM_EXEC:
        # Exclude obvious non-code classes if class is set
        cls = (ida_segment.get_segm_class(seg) or "").upper()
        if cls in ("DATA", "BSS", "STACK", "HEAP"):
            return False
        return True
    return False


def _discover_code_segments():
    """All CODE (executable) segments, largest first."""
    segs = [s for s in _iter_all_segments() if _is_code_segment(s)]
    segs.sort(key=lambda s: -(s.end_ea - s.start_ea))
    return segs


def _find_segment_by_name(name):
    name = name.strip()
    if not name:
        return None
    # Exact match
    for seg in _iter_all_segments():
        if (ida_segment.get_segm_name(seg) or "").strip() == name:
            return seg
    # seg_400000 style alias for unnamed sections
    if name.startswith("seg_"):
        try:
            start = int(name[4:], 16)
        except ValueError:
            return None
        for seg in _iter_all_segments():
            if seg.start_ea == start:
                return seg
    return None


def _resolve_segments(spec):
    """
    Parse TEXTWALL_SEGMENT spec into a list of segment_t.
    spec: 'auto' | '.text' | 'seg_00401000,seg_00A00000' | empty -> auto
    """
    if spec is None:
        spec = "auto"
    spec = spec.strip()
    if not spec or spec.lower() == "auto":
        segs = _discover_code_segments()
        if not segs:
            return None, "auto: no CODE segments found"
        return segs, "auto"

    if spec.lower() == ".text":
        seg = _find_segment_by_name(".text")
        if seg:
            return [seg], ".text"
        # PE often has no .text; fall back to auto for GAME-like binaries
        segs = _discover_code_segments()
        if segs:
            print("NOTE: .text not found; using auto CODE segments (%d)" % len(segs))
            return segs, "auto"
        return None, "segment .text not found"

    segs = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        seg = _find_segment_by_name(part)
        if not seg:
            return None, "segment %r not found" % part
        segs.append(seg)
    if not segs:
        return None, "empty segment list"
    return segs, spec


def _list_segments():
    print("TEXTWALL_SEGMENTS")
    for seg in _iter_all_segments():
        info = _seg_info_dict(seg)
        code = " CODE" if _is_code_segment(seg) else ""
        print(
            "  %-16s  %s-%s  size=0x%X  class=%s%s"
            % (
                info["name"],
                info["start"],
                info["end"],
                info["size"],
                info["class"],
                code,
            )
        )
    code_segs = _discover_code_segments()
    print("Auto CODE segments (%d):" % len(code_segs))
    for seg in code_segs:
        print("  -> %s" % _seg_display_name(seg))


def _in_segments(ea, segments):
    for seg in segments:
        if seg.start_ea <= ea < seg.end_ea:
            return True
    return False


def _is_lib_thunk(fn):
    """Skip import stubs / thunks. Do not use 0x4000 — that is not FUNC_LIB on IDA 9."""
    flags = getattr(fn, "flags", 0)
    if flags & ida_funcs.FUNC_LIB:
        return True
    if getattr(ida_funcs, "FUNC_THUNK", 0) and (flags & ida_funcs.FUNC_THUNK):
        return True
    name = ida_funcs.get_func_name(fn.start_ea) or ""
    if name.startswith("j_") or name.startswith("__imp_"):
        return True
    return False


def _collect_functions(segments, include_thunks):
    ea_to_i = {}
    nodes = []
    skipped = {"lib": 0}
    for start in idautils.Functions():
        if not _in_segments(start, segments):
            continue
        f = ida_funcs.get_func(start)
        if not f:
            continue
        if not include_thunks and _is_lib_thunk(f):
            skipped["lib"] += 1
            continue
        idx = len(nodes)
        ea_to_i[start] = idx
        name = ida_funcs.get_func_name(start) or idc.get_name(start) or ""
        size = int(f.end_ea - f.start_ea)
        nodes.append({"i": idx, "ea": "0x%X" % start, "name": name, "size": size})
    return nodes, ea_to_i, skipped


def _collect_edges(ea_to_i, include_thunks):
    edges_set = set()
    starts = list(ea_to_i.keys())
    total = len(starts)
    report_every = max(1, total // 25)

    for idx, start in enumerate(starts):
        if idx % report_every == 0:
            print("  edge scan %d/%d (%d edges)" % (idx, total, len(edges_set)))

        caller_i = ea_to_i[start]
        f = ida_funcs.get_func(start)
        if not f:
            continue
        if not include_thunks and _is_lib_thunk(f):
            continue

        for head in idautils.FuncItems(start):
            # CodeRefsFrom is much faster than filtering all XrefsFrom on large IDBs
            for ref_ea in idautils.CodeRefsFrom(head, 0):
                callee_f = ida_funcs.get_func(ref_ea)
                if not callee_f:
                    continue
                callee_start = callee_f.start_ea
                if callee_start not in ea_to_i:
                    continue
                callee_i = ea_to_i[callee_start]
                if callee_i == caller_i:
                    continue
                edges_set.add((caller_i, callee_i))

    return sorted(edges_set)


def main():
    if _env("TEXTWALL_LIST_SEGMENTS", "0") in ("1", "true", "yes"):
        _list_segments()
        return 0

    seg_spec = _env("TEXTWALL_SEGMENT", "auto")
    out_path = _env("TEXTWALL_OUT", _default_out_path())
    include_thunks = _env("TEXTWALL_INCLUDE_LIB_THUNKS", "0") in ("1", "true", "yes")

    segments, seg_label = _resolve_segments(seg_spec)
    if segments is None:
        print("ERROR: %s" % seg_label)
        print("Hint: TEXTWALL_LIST_SEGMENTS=1 to print segments; TEXTWALL_SEGMENT=auto (default)")
        return 1

    image_base = idc.get_imagebase() if hasattr(idc, "get_imagebase") else 0x400000

    nodes, ea_to_i, skipped = _collect_functions(segments, include_thunks)
    edges = _collect_edges(ea_to_i, include_thunks)
    total_funcs = sum(1 for _ in idautils.Functions())

    idb_path = idc.get_idb_path() if hasattr(idc, "get_idb_path") else ""
    seg_meta = [_seg_info_dict(s) for s in segments]

    payload = {
        "meta": {
            "idb": os.path.basename(idb_path),
            "idb_path": idb_path,
            "image_base": "0x%X" % image_base,
            "segment": seg_label,
            "segments": seg_meta,
            "node_count": len(nodes),
            "edge_count": len(edges),
            "export_version": EXPORT_VERSION,
            "include_lib_thunks": include_thunks,
            "ida_function_count": total_funcs,
            "skipped_lib_or_thunk": skipped["lib"],
        },
        "nodes": nodes,
        "edges": [[u, v] for u, v in edges],
    }

    out_dir = os.path.dirname(out_path)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    if out_path.endswith(".gz"):
        with gzip.open(out_path, "wt", encoding="utf-8") as f:
            json.dump(payload, f, separators=(",", ":"))
    else:
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(payload, f, separators=(",", ":"))

    print("TEXTWALL_EXPORT_OK")
    print("  nodes=%d edges=%d (ida_functions=%d)" % (len(nodes), len(edges), total_funcs))
    if not include_thunks:
        print("  skipped: lib/thunk/import_name=%d" % skipped["lib"])
    print("  segments=%s (%d)" % (seg_label, len(segments)))
    for info in seg_meta:
        print("    %s %s-%s" % (info["name"], info["start"], info["end"]))
    print("  thunks=%s" % include_thunks)
    print("  out=%s" % out_path)
    return 0


if __name__ == "__main__":
    try:
        rc = main()
    except Exception as e:
        print("TEXTWALL_EXPORT_FAIL: %s" % e)
        import traceback
        traceback.print_exc()
        rc = 1
    if rc:
        print("exit code", rc)
