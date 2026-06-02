"""
Apply cluster colors/comments from clusters.tsv (textwall-graph output).

Environment:
  TEXTWALL_TSV   path to clusters.tsv (required)
  TEXTWALL_PREFIX comment prefix (default: cluster:)

Run inside IDA after clustering.
"""
from __future__ import print_function

import os

import ida_funcs
import idc

# IDA color indices (0-0xFF); cycle for cluster ids
_PALETTE = [
    0x98D7FF,
    0x98FFD7,
    0xD7FF98,
    0xFFD798,
    0xFF98D7,
    0xD798FF,
    0xC0C0C0,
    0xE0E0E0,
]


def _env(name, default=None):
    v = os.environ.get(name)
    return v if v is not None and v != "" else default


def _parse_rva(s):
    s = s.strip()
    if s.lower().startswith("0x"):
        return int(s, 16)
    return int(s, 0)


def _ea_from_rva(rva):
    base = idc.get_imagebase() if hasattr(idc, "get_imagebase") else 0x400000
    return base + rva


def main():
    tsv_path = _env("TEXTWALL_TSV")
    if not tsv_path or not os.path.isfile(tsv_path):
        print("ERROR: set TEXTWALL_TSV to clusters.tsv path")
        return 1

    prefix = _env("TEXTWALL_PREFIX", "cluster:")

    applied = 0
    with open(tsv_path, "r", encoding="utf-8") as f:
        header = f.readline()
        if not header.lower().startswith("cluster_id"):
            print("WARN: unexpected header:", header.strip())

        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) < 3:
                continue
            cluster_id = int(parts[0])
            rva_s = parts[1]
            tea_s = parts[2] if len(parts) > 2 else None

            if tea_s and tea_s.lower().startswith("0x"):
                ea = int(tea_s, 16)
            else:
                ea = _ea_from_rva(_parse_rva(rva_s))

            fn = ida_funcs.get_func(ea)
            if not fn:
                continue
            start = fn.start_ea

            color = _PALETTE[cluster_id % len(_PALETTE)]
            if hasattr(ida_funcs, "set_func_color"):
                ida_funcs.set_func_color(start, color)
            elif hasattr(idc, "set_color"):
                idc.set_color(start, idc.CIC_FUNC, color)

            old = idc.get_func_cmt(start, 0) or ""
            tag = "%s%d" % (prefix, cluster_id)
            if tag not in old:
                new_cmt = (tag + " " + old).strip() if old else tag
                idc.set_func_cmt(start, new_cmt, 0)

            applied += 1

    print("TEXTWALL_APPLY_OK applied=%d from %s" % (applied, tsv_path))
    return 0


if __name__ == "__main__":
    try:
        rc = main()
    except Exception as e:
        print("TEXTWALL_APPLY_FAIL:", e)
        import traceback
        traceback.print_exc()
        rc = 1
