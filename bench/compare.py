#!/usr/bin/env python3
"""
Compare two benchmark runs (micro or live json) by name.

  python3 bench/compare.py results/base.json results/new.json [--threshold 10]

A benchmark counts as a regression when its time per operation grew by more
than the threshold and by more than its own noise; the same for daemon CPU in
live runs and for allocations per operation in micro runs. The exit status is
1 when anything regressed, so the script can gate a change.
"""

import argparse
import json
import sys

METRICS = {
    "micro": [("ns_op", "time/op", "ns"), ("cpu_ns_op", "cpu/op", "ns"), ("allocs_op", "allocs/op", "n"), ("bytes_op", "bytes/op", "n")],
    "live": [("ns_op", "p50", "ns"), ("ns_p95", "p95", "ns"), ("daemon_cpu_ns_op", "daemon cpu", "ns"), ("daemon_wakeups_op", "wakeups", "n")],
}
# NOTE: the metrics a regression is judged on; the rest are shown for context.
GATED = {"micro": ("ns_op", "allocs_op"), "live": ("ns_op", "daemon_cpu_ns_op")}


def fmt(value, unit):
    if unit == "n":
        return "%.1f" % value
    if value < 1e3:
        return "%.1f ns" % value
    if value < 1e6:
        return "%.2f µs" % (value / 1e3)
    return "%.2f ms" % (value / 1e6)


def noise_pct(r):
    if "cv_pct" in r:
        return r["cv_pct"]
    if r.get("ns_op") and "ns_p95" in r:
        return 100.0 * (r["ns_p95"] - r["ns_op"]) / r["ns_op"] / 2
    return 0.0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("base")
    ap.add_argument("new")
    ap.add_argument("--threshold", type=float, default=10.0, help="percent growth that counts as a regression (default 10)")
    args = ap.parse_args()

    base, new = (json.load(open(p)) for p in (args.base, args.new))
    kind = new["meta"].get("kind", "micro")
    if base["meta"].get("kind", "micro") != kind:
        sys.exit("compare: %s is a %s run and %s a %s run" % (args.base, base["meta"].get("kind"), args.new, kind))

    old = {r["name"]: r for r in base["results"]}
    metrics = METRICS[kind]
    print("base %s (%s)  →  new %s (%s)\n" % (args.base, base["meta"].get("git", base["meta"].get("yabai", "?")),
                                              args.new, new["meta"].get("git", new["meta"].get("yabai", "?"))))
    header = "%-30s" % "benchmark" + "".join(" %24s" % label for _, label, _ in metrics)
    print(header)

    regressions, improvements = [], []
    for r in new["results"]:
        o = old.get(r["name"])
        if not o:
            print("%-30s   (new)" % r["name"])
            continue
        cells = []
        noise = max(noise_pct(o), noise_pct(r))
        for key, label, unit in metrics:
            a, b = o.get(key, 0), r.get(key, 0)
            if a == 0 and b == 0:
                cells.append(" %24s" % "—")
                continue
            pct = 100.0 * (b - a) / a if a else float("inf")
            mark = ""
            if key in GATED[kind]:
                limit = args.threshold if unit == "n" else max(args.threshold, 2 * noise)
                if pct > limit and (unit != "n" or b - a >= 1):
                    mark = " ▲"
                    regressions.append((r["name"], label, pct))
                elif pct < -limit:
                    mark = " ▼"
                    improvements.append((r["name"], label, pct))
            cells.append(" %24s" % ("%s %+.0f%%%s" % (fmt(b, unit), pct, mark)))
        print("%-30s%s" % (r["name"], "".join(cells)))

    seen = {r["name"] for r in new["results"]}
    gone = [name for name in old if name not in seen]
    if gone:
        print("\n%d only in base: %s" % (len(gone), ", ".join(gone) if len(gone) <= 8 else ", ".join(gone[:8]) + ", …"))

    print()
    if improvements:
        print("%d faster: %s" % (len(improvements), ", ".join("%s %s %+.0f%%" % i for i in improvements)))
    if regressions:
        print("%d regressed (> %.0f%% and above noise): %s" % (len(regressions), args.threshold, ", ".join("%s %s %+.0f%%" % i for i in regressions)))
        sys.exit(1)
    print("no regressions above %.0f%%" % args.threshold)


if __name__ == "__main__":
    main()
