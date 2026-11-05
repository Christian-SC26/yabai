#!/usr/bin/env python3
"""
Live benchmark of the running yabai daemon.

It talks to the daemon over its socket the way `yabai -m` does, and reads the
daemon's own resource counters (proc_pid_rusage, proc_pidinfo) around every
batch, so each command gets:

  * latency per call: p50 / p95 / p99 / min, measured at the client
  * daemon CPU per call (user + system), including work done after the answer
  * daemon wakeups, mach messages, unix syscalls and context switches per call
  * daemon instructions and energy per call where the kernel reports them
  * the daemon's memory footprint before and after, to spot growth

Scenarios:
  idle     the daemon left alone for --idle seconds
  read     queries and getters; nothing on screen changes (always runs)
  cli      the same query through the `yabai -m` binary, process spawn included
  mutate   only with --mutate: a new space with windows the benchmark owns,
           on which it swaps, warps, focuses, resizes, rotates, relayouts,
           opens and closes windows; the space is destroyed and the space
           and window that had focus get it back afterwards
"""

import argparse
import ctypes
import ctypes.util
import json
import os
import platform
import shutil
import socket
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HELPER = os.path.join(HERE, "bin", "bench-windows.app", "Contents", "MacOS", "bench-windows")
HELPER_APP = "bench-windows"
FAILURE = 0x07

# ---------------------------------------------------------------------------
# daemon counters
# ---------------------------------------------------------------------------

libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)


class RUsageInfoV4(ctypes.Structure):
    _fields_ = [("ri_uuid", ctypes.c_uint8 * 16)] + [(name, ctypes.c_uint64) for name in (
        "ri_user_time", "ri_system_time", "ri_pkg_idle_wkups", "ri_interrupt_wkups",
        "ri_pageins", "ri_wired_size", "ri_resident_size", "ri_phys_footprint",
        "ri_proc_start_abstime", "ri_proc_exit_abstime", "ri_child_user_time",
        "ri_child_system_time", "ri_child_pkg_idle_wkups", "ri_child_interrupt_wkups",
        "ri_child_pageins", "ri_child_elapsed_abstime", "ri_diskio_bytesread",
        "ri_diskio_byteswritten", "ri_cpu_time_qos_default", "ri_cpu_time_qos_maintenance",
        "ri_cpu_time_qos_background", "ri_cpu_time_qos_utility", "ri_cpu_time_qos_legacy",
        "ri_cpu_time_qos_user_initiated", "ri_cpu_time_qos_user_interactive",
        "ri_billed_system_time", "ri_serviced_system_time", "ri_logical_writes",
        "ri_lifetime_max_phys_footprint", "ri_instructions", "ri_cycles",
        "ri_billed_energy", "ri_serviced_energy", "ri_interval_max_phys_footprint",
        "ri_runnable_time")]


class ProcTaskInfo(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint64) for name in (
        "pti_virtual_size", "pti_resident_size", "pti_total_user", "pti_total_system",
        "pti_threads_user", "pti_threads_system")] + [(name, ctypes.c_int32) for name in (
        "pti_policy", "pti_faults", "pti_pageins", "pti_cow_faults", "pti_messages_sent",
        "pti_messages_received", "pti_syscalls_mach", "pti_syscalls_unix", "pti_csw",
        "pti_threadnum", "pti_numrunning", "pti_priority")]


class MachTimebase(ctypes.Structure):
    _fields_ = [("numer", ctypes.c_uint32), ("denom", ctypes.c_uint32)]


_timebase = MachTimebase()
libc.mach_timebase_info(ctypes.byref(_timebase))
ABS_TO_NS = _timebase.numer / _timebase.denom
RUSAGE_INFO_V4 = 4
PROC_PIDTASKINFO = 4


def daemon_counters(pid):
    ri = RUsageInfoV4()
    if libc.proc_pid_rusage(pid, RUSAGE_INFO_V4, ctypes.byref(ri)) != 0:
        raise OSError(ctypes.get_errno(), "proc_pid_rusage failed; is yabai running?")
    ti = ProcTaskInfo()
    libc.proc_pidinfo(pid, PROC_PIDTASKINFO, ctypes.c_uint64(0), ctypes.byref(ti), ctypes.sizeof(ti))
    return {
        "cpu_ns": (ri.ri_user_time + ri.ri_system_time) * ABS_TO_NS,
        "wakeups": ri.ri_pkg_idle_wkups + ri.ri_interrupt_wkups,
        "instructions": ri.ri_instructions,
        "cycles": ri.ri_cycles,
        "energy_nj": ri.ri_billed_energy,
        "footprint": ri.ri_phys_footprint,
        "footprint_peak": ri.ri_lifetime_max_phys_footprint,
        "mach_msgs": ti.pti_messages_sent + ti.pti_messages_received,
        "syscalls": ti.pti_syscalls_mach + ti.pti_syscalls_unix,
        "csw": ti.pti_csw,
        "threads": ti.pti_threadnum,
    }


def counters_delta(a, b):
    return {k: b[k] - a[k] for k in ("cpu_ns", "wakeups", "instructions", "cycles", "energy_nj", "mach_msgs", "syscalls", "csw")}


# ---------------------------------------------------------------------------
# talking to the daemon
# ---------------------------------------------------------------------------

def socket_path():
    return "/tmp/yabai_%s.socket" % os.environ["USER"]


def send(*args):
    """One `yabai -m` call over the socket; returns (ok, text)."""
    payload = b"".join(a.encode() + b"\0" for a in args) + b"\0"
    msg = (len(payload)).to_bytes(4, "little") + payload
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.connect(socket_path())
        s.sendall(msg)
        s.shutdown(socket.SHUT_WR)
        chunks = []
        while True:
            data = s.recv(65536)
            if not data:
                break
            chunks.append(data)
    out = b"".join(chunks)
    if out[:1] == bytes([FAILURE]):
        return False, out[1:].decode(errors="replace").strip()
    return True, out.decode(errors="replace")


def query(*args):
    ok, text = send("query", *args)
    if not ok:
        raise RuntimeError("query %s failed: %s" % (" ".join(args), text))
    return json.loads(text)


def daemon_pid():
    out = subprocess.run(["pgrep", "-x", "yabai"], capture_output=True, text=True).stdout.split()
    if not out:
        sys.exit("live: yabai is not running")
    return int(out[0])


# ---------------------------------------------------------------------------
# measuring
# ---------------------------------------------------------------------------

def percentile(sorted_values, q):
    if not sorted_values:
        return 0.0
    pos = q * (len(sorted_values) - 1)
    lo = int(pos)
    hi = min(lo + 1, len(sorted_values) - 1)
    return sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * (pos - lo)


class Bench:
    def __init__(self, pid, settle, idle_rate):
        self.pid = pid
        self.settle = settle
        self.idle_rate = idle_rate   # daemon counters per second while idle
        self.results = []

    def measure(self, name, calls, warmup=2, note=None):
        """calls: list of zero-argument callables, run round robin.

        Warmup and measured calls are whole even rounds of the list, so a
        toggle or a pair of opposite commands leaves the windows as it found
        them for the next benchmark."""
        for i in range(warmup * len(calls)):
            calls[i % len(calls)]()
        if self.settle:
            time.sleep(self.settle)

        latencies = []
        failures = []
        failed = 0
        before = daemon_counters(self.pid)
        t_begin = time.perf_counter()
        rounds = 2 * len(calls)
        iterations = -(-self.iterations_for(name) // rounds) * rounds
        for i in range(iterations):
            t0 = time.perf_counter_ns()
            ok, text = calls[i % len(calls)]()
            latencies.append(time.perf_counter_ns() - t0)
            if not ok:
                failed += 1
                if len(failures) < 3:
                    failures.append(text)
        if self.settle:
            time.sleep(self.settle)
        elapsed = time.perf_counter() - t_begin
        after = daemon_counters(self.pid)

        n = len(latencies)
        delta = counters_delta(before, after)
        # NOTE: what the daemon spends anyway while idle over the same span is not the command's.
        per_op = {k: max(0.0, (delta[k] - self.idle_rate.get(k, 0) * elapsed)) / n for k in delta}
        latencies.sort()
        r = {
            "name": name,
            "n": n,
            "ns_op": percentile(latencies, 0.5),
            "ns_p95": percentile(latencies, 0.95),
            "ns_p99": percentile(latencies, 0.99),
            "ns_min": latencies[0],
            "ns_mean": statistics.fmean(latencies),
            "daemon_cpu_ns_op": per_op["cpu_ns"],
            "daemon_wakeups_op": per_op["wakeups"],
            "daemon_mach_msgs_op": per_op["mach_msgs"],
            "daemon_syscalls_op": per_op["syscalls"],
            "daemon_csw_op": per_op["csw"],
            "daemon_instructions_op": per_op["instructions"],
            "daemon_energy_nj_op": per_op["energy_nj"],
            "footprint_delta_bytes": after["footprint"] - before["footprint"],
        }
        if failures:
            r["failed"] = failed
            r["failures"] = failures
        if note:
            r["note"] = note
        self.results.append(r)
        print_row(r)
        if note and os.environ.get("BENCH_DEBUG"):
            print("    note:", note)
        return r

    def iterations_for(self, name):
        group = name.split("/")[0]
        return self.iters.get(group, self.iters["default"])


def fmt_ns(ns):
    if ns < 1e3:
        return "%.0f ns" % ns
    if ns < 1e6:
        return "%.1f µs" % (ns / 1e3)
    return "%.2f ms" % (ns / 1e6)


def print_header():
    print("%-30s %5s %10s %10s %10s %11s %8s %8s %8s %9s" % (
        "benchmark", "n", "p50", "p95", "p99", "daemon cpu", "wakeups", "mach msg", "syscalls", "Δ memory"))


def print_row(r):
    flag = "  ! %d/%d failed: %s" % (r["failed"], r["n"], r["failures"][0]) if "failures" in r else ""
    print("%-30s %5d %10s %10s %10s %11s %8.1f %8.1f %8.1f %8.0fK%s" % (
        r["name"], r["n"], fmt_ns(r["ns_op"]), fmt_ns(r["ns_p95"]), fmt_ns(r["ns_p99"]),
        fmt_ns(r["daemon_cpu_ns_op"]), r["daemon_wakeups_op"], r["daemon_mach_msgs_op"],
        r["daemon_syscalls_op"], r["footprint_delta_bytes"] / 1024, flag))
    sys.stdout.flush()


def measure_idle(pid, seconds):
    before = daemon_counters(pid)
    t0 = time.perf_counter()
    time.sleep(seconds)
    elapsed = time.perf_counter() - t0
    after = daemon_counters(pid)
    delta = counters_delta(before, after)
    rate = {k: v / elapsed for k, v in delta.items()}
    idle = {
        "seconds": elapsed,
        "cpu_pct": 100.0 * rate["cpu_ns"] / 1e9,
        "wakeups_per_s": rate["wakeups"],
        "mach_msgs_per_s": rate["mach_msgs"],
        "syscalls_per_s": rate["syscalls"],
        "energy_nj_per_s": rate["energy_nj"],
        "footprint_bytes": after["footprint"],
        "footprint_peak_bytes": after["footprint_peak"],
        "threads": after["threads"],
    }
    print("idle %.1fs: cpu %.3f%%, %.1f wakeups/s, %.1f mach msgs/s, %.1f syscalls/s, footprint %.1f MB (peak %.1f MB), %d threads" % (
        elapsed, idle["cpu_pct"], idle["wakeups_per_s"], idle["mach_msgs_per_s"], idle["syscalls_per_s"],
        idle["footprint_bytes"] / 1048576, idle["footprint_peak_bytes"] / 1048576, idle["threads"]))
    return idle, rate


# ---------------------------------------------------------------------------
# scenarios
# ---------------------------------------------------------------------------

def call(*args):
    return lambda: send(*args)


def scenario_read(bench):
    print("\n-- read: queries and getters")
    bench.measure("read/query_displays", [call("query", "--displays")])
    bench.measure("read/query_display", [call("query", "--displays", "--display")])
    bench.measure("read/query_spaces", [call("query", "--spaces")])
    bench.measure("read/query_space", [call("query", "--spaces", "--space")])
    bench.measure("read/query_windows", [call("query", "--windows")])
    bench.measure("read/query_windows_props", [call("query", "--windows", "id,app,space")])
    bench.measure("read/query_space_windows", [call("query", "--windows", "--space")])
    focused = send("query", "--windows", "--window")
    if focused[0]:
        bench.measure("read/query_window", [call("query", "--windows", "--window")])
    bench.measure("read/config_get", [call("config", "layout"), call("config", "window_gap"), call("config", "split_ratio")])
    bench.measure("read/rule_list", [call("rule", "--list")])
    bench.measure("read/signal_list", [call("signal", "--list")])


def scenario_cli(bench):
    yabai = shutil.which("yabai")
    if not yabai:
        print("\n-- cli: skipped, no yabai binary on PATH")
        return
    print("\n-- cli: `yabai -m` including process spawn (%s)" % yabai)

    def run(*args):
        def f():
            p = subprocess.run([yabai, "-m", *args], capture_output=True)
            return p.returncode == 0, p.stderr.decode(errors="replace").strip()
        return f

    bench.measure("cli/query_space", [run("query", "--spaces", "--space")])
    bench.measure("cli/config_get", [run("config", "layout")])


class BenchSpace:
    """A new space holding windows the benchmark owns, removed on exit."""

    def __init__(self, count):
        self.count = count
        self.helper = None
        self.sid = None
        self.orig_sid = query("--spaces", "--space")["id"]
        ok, text = send("query", "--windows", "--window")
        self.orig_wid = json.loads(text)["id"] if ok else None

    def index_of(self, sid):
        for s in query("--spaces"):
            if s["id"] == sid:
                return s["index"]
        return None

    def __enter__(self):
        before = {s["id"] for s in query("--spaces")}
        ok, text = send("space", "--create")
        if not ok:
            raise RuntimeError("space --create failed (is the scripting addition loaded?): " + text)
        created = [s for s in query("--spaces") if s["id"] not in before]
        if not created:
            raise RuntimeError("space --create did not add a space")
        self.sid = created[0]["id"]
        ok, text = send("space", "--focus", str(self.index_of(self.sid)))
        if not ok:
            raise RuntimeError("space --focus failed: " + text)
        wait_until(lambda: query("--spaces", "--space")["id"] == self.sid, 3, "the bench space to get focus")
        send("space", str(self.index_of(self.sid)), "--layout", "bsp")

        self.helper = subprocess.Popen([HELPER, str(self.count)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.wids = []
        for line in self.helper.stdout:
            if line.startswith("window "):
                self.wids.append(int(line.split()[1]))
            elif line.strip() == "ready":
                break
        # NOTE: windows of an earlier run can linger in yabai for a moment, so
        # wait for exactly the windows this helper opened, not for a count.
        wait_until(lambda: {w["id"] for w in self.tiled()} == set(self.wids), 5, "yabai to tile the bench windows")
        return self

    def tiled(self):
        return [w for w in query("--windows", "--space", str(self.index_of(self.sid)))
                if w["app"] == HELPER_APP and w["has-ax-reference"] and not w["is-floating"]]

    def helper_cmd(self, line):
        self.helper.stdin.write(line + "\n")
        self.helper.stdin.flush()
        return self.helper.stdout.readline().split()

    def __exit__(self, *exc):
        if self.helper:
            try:
                self.helper.stdin.write("quit\n")
                self.helper.stdin.flush()
                self.helper.wait(3)
            except Exception:
                self.helper.kill()
        if self.sid:
            orig_index = self.index_of(self.orig_sid)
            if orig_index:
                send("space", "--focus", str(orig_index))
            index = self.index_of(self.sid)
            if index:
                ok, text = send("space", str(index), "--destroy")
                if not ok:
                    print("live: could not destroy the bench space %s: %s" % (index, text), file=sys.stderr)
        if self.orig_wid:
            send("window", "--focus", str(self.orig_wid))
        return False


def wait_until(predicate, timeout, what):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise RuntimeError("timed out waiting for " + what)


def scenario_mutate(bench, count):
    print("\n-- mutate: %d windows the benchmark owns on a new space" % count)
    with BenchSpace(count) as bs:
        a, b = bs.wids[0], bs.wids[1]
        index = lambda: str(bs.index_of(bs.sid))
        space = index()
        A, B = str(a), str(b)

        bench.measure("window/focus", [call("window", "--focus", A), call("window", "--focus", B)])
        bench.measure("window/swap", [call("window", A, "--swap", B)])
        bench.measure("window/warp", [call("window", A, "--warp", B), call("window", B, "--warp", A)])
        bench.measure("window/ratio", [call("window", A, "--ratio", "abs:0.4"), call("window", A, "--ratio", "abs:0.6")])
        # NOTE: a resize needs a fence on that edge. Probing with a resize is not
        # reliable, since yabai finds fences by comparing float edges, so the
        # edge is picked from where the window sits on the display.
        f = query("--windows", "--window", A)["frame"]
        d = query("--displays", "--display")["frame"]
        if f["x"] > d["x"] + 50:
            edge, shrink, grow = "left", "20:0", "-20:0"
        elif f["x"] + f["w"] < d["x"] + d["w"] - 50:
            edge, shrink, grow = "right", "-20:0", "20:0"
        elif f["y"] > d["y"] + 50:
            edge, shrink, grow = "top", "0:20", "0:-20"
        else:
            edge, shrink, grow = "bottom", "0:-20", "0:20"
        bench.measure("window/resize", [call("window", A, "--resize", "%s:%s" % (edge, shrink)), call("window", A, "--resize", "%s:%s" % (edge, grow))],
                      note="resizes the %s edge" % edge)
        bench.measure("window/toggle_float", [call("window", A, "--toggle", "float")])
        bench.measure("window/toggle_zoom", [call("window", A, "--toggle", "zoom-fullscreen")])
        bench.measure("window/toggle_split", [call("window", A, "--toggle", "split")])
        bench.measure("space/rotate", [call("space", space, "--rotate", "90")])
        bench.measure("space/mirror", [call("space", space, "--mirror", "y-axis"), call("space", space, "--mirror", "x-axis")])
        bench.measure("space/balance", [call("space", space, "--balance")])
        bench.measure("space/gap", [call("space", space, "--gap", "abs:8"), call("space", space, "--gap", "abs:12")])
        bench.measure("space/padding", [call("space", space, "--padding", "abs:8:8:8:8"), call("space", space, "--padding", "abs:12:12:12:12")])
        bench.measure("space/layout", [call("space", space, "--layout", "stack"), call("space", space, "--layout", "bsp")])

        # NOTE: a window opened and closed by its application, timed from the
        # request to the moment yabai tiles it and from closing it to the
        # moment yabai lets it go: the event path rather than a command.
        def open_close():
            t0 = time.perf_counter_ns()
            wid = int(bs.helper_cmd("open")[1])
            wait_until(lambda: any(w["id"] == wid for w in bs.tiled()), 5, "a new window to be tiled")
            bs.helper_cmd("close %d" % wid)
            wait_until(lambda: send("query", "--windows", "--window", str(wid))[0] is False, 5, "a closed window to go")
            return True, str(time.perf_counter_ns() - t0)

        bench.measure("event/open_tile_close", [open_close], warmup=1,
                      note="latency is window open → tiled → closed → released, polled every 20ms; daemon counters include the polling queries")
        send("space", space, "--layout", "bsp")


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--iterations", type=int, default=300, help="calls per read benchmark (default 300)")
    ap.add_argument("--mutate-iterations", type=int, default=40, help="calls per mutating benchmark (default 40)")
    ap.add_argument("--cli-iterations", type=int, default=50, help="calls per `yabai -m` benchmark (default 50)")
    ap.add_argument("--idle", type=float, default=5.0, help="seconds to watch the idle daemon (default 5, 0 skips)")
    ap.add_argument("--settle", type=float, default=0.3, help="seconds to let async work finish around a batch (default 0.3)")
    ap.add_argument("--mutate", action="store_true", help="also run the scenarios that move windows on a new space")
    ap.add_argument("--windows", type=int, default=6, help="windows on the bench space (default 6)")
    ap.add_argument("--filter", action="append", help="run only scenario groups whose name contains this (idle, read, cli, mutate)")
    ap.add_argument("--json", help="also write the results as json, for compare.py")
    args = ap.parse_args()

    wanted = lambda g: not args.filter or any(f in g for f in args.filter)
    pid = daemon_pid()
    version = subprocess.run(["yabai", "--version"], capture_output=True, text=True).stdout.strip() if shutil.which("yabai") else "?"
    start = daemon_counters(pid)
    print("yabai live benchmark — pid %d, %s, %d threads, footprint %.1f MB" % (pid, version, start["threads"], start["footprint"] / 1048576))

    idle, idle_rate = (None, {})
    if args.idle > 0 and wanted("idle"):
        idle, idle_rate = measure_idle(pid, args.idle)

    bench = Bench(pid, args.settle, idle_rate)
    bench.iters = {"default": args.iterations, "cli": args.cli_iterations, "window": args.mutate_iterations,
                   "space": args.mutate_iterations, "event": max(5, args.mutate_iterations // 4)}
    print()
    print_header()
    if wanted("read"):
        scenario_read(bench)
    if wanted("cli"):
        scenario_cli(bench)
    if args.mutate and wanted("mutate"):
        if not os.path.exists(HELPER):
            sys.exit("live: %s is missing, run `make -C bench build` first" % HELPER)
        scenario_mutate(bench, args.windows)

    end = daemon_counters(pid)
    print("\ndaemon footprint %.1f MB → %.1f MB (peak %.1f MB), %d threads" % (
        start["footprint"] / 1048576, end["footprint"] / 1048576, end["footprint_peak"] / 1048576, end["threads"]))

    if args.json:
        meta = {"kind": "live", "yabai": version, "time": int(time.time()), "machine": platform.machine(),
                "os": platform.mac_ver()[0], "footprint_start_bytes": start["footprint"],
                "footprint_end_bytes": end["footprint"], "footprint_peak_bytes": end["footprint_peak"],
                "threads": end["threads"], "idle": idle}
        with open(args.json, "w") as f:
            json.dump({"meta": meta, "results": bench.results}, f, indent=2)
        print("results written to %s" % args.json)


if __name__ == "__main__":
    main()
