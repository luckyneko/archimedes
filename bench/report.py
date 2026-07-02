#!/usr/bin/env python3
"""
Run bench-archimedes and print a compact Catch2 benchmark summary.

Usage:
    python3 bench/report.py [--exec <path>] [--json-out <path>] [--json-append <path>] [--compare <path>] [bench-archimedes args...]

Examples:
    python3 bench/report.py "[fast]"
    python3 bench/report.py "[bench]" --benchmark-samples=30
    python3 bench/report.py "[fast]" --json-out build/bench-latest.json
    python3 bench/report.py "[fast]" --json-append build/bench-history.json
    python3 bench/report.py "[fast]" --compare build/bench-history.json --json-append build/bench-history.json
    python3 bench/report.py --exec build-release/bench-archimedes "[bench]"
"""

import datetime
import json
import math
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from typing import Any, Dict, List, Optional, Tuple

DEFAULT_EXEC = "./build/bench-archimedes"
RESET = "\033[0m"
DIM = "\033[2m"
GREEN = "\033[32m"
RED = "\033[31m"
BOLD_GREEN = "\033[1;32m"
BOLD_RED = "\033[1;31m"


def candidate_execs(exec_path: str) -> List[str]:
    head, tail = os.path.split(exec_path)
    bases = [exec_path]
    for config in ("Release", "RelWithDebInfo", "Debug"):
        bases.append(os.path.join(head, config, tail))

    suffixes = ["", ".exe"] if os.name == "nt" else [""]

    seen = set()
    out: List[str] = []
    for base in bases:
        for suffix in suffixes:
            cand = base + suffix
            if cand not in seen:
                seen.add(cand)
                out.append(cand)
    return out


def resolve_exec(exec_path: str) -> Optional[str]:
    for cand in candidate_execs(exec_path):
        if os.path.isfile(cand):
            return cand
    return None


def fmt_time(ns: float) -> str:
    if ns >= 1e9:
        return f"{ns / 1e9:.3f} s"
    if ns >= 1e6:
        return f"{ns / 1e6:.3f} ms"
    if ns >= 1e3:
        return f"{ns / 1e3:.1f} us"
    return f"{ns:.1f} ns"


def split_args(argv: List[str]) -> Tuple[str, Optional[str], Optional[str], Optional[str], List[str]]:
    exec_path = DEFAULT_EXEC
    json_out: Optional[str] = None
    json_append: Optional[str] = None
    compare_path: Optional[str] = None
    rest: List[str] = []
    i = 0
    while i < len(argv):
        if argv[i] == "--exec":
            if i + 1 >= len(argv):
                sys.exit("error: --exec requires a path argument")
            exec_path = argv[i + 1]
            i += 2
        elif argv[i] == "--json-out":
            if i + 1 >= len(argv):
                sys.exit("error: --json-out requires a path argument")
            json_out = argv[i + 1]
            i += 2
        elif argv[i] == "--json-append":
            if i + 1 >= len(argv):
                sys.exit("error: --json-append requires a path argument")
            json_append = argv[i + 1]
            i += 2
        elif argv[i] == "--compare":
            if i + 1 >= len(argv):
                sys.exit("error: --compare requires a path argument")
            compare_path = argv[i + 1]
            i += 2
        else:
            rest.append(argv[i])
            i += 1
    return exec_path, json_out, json_append, compare_path, rest


def run_bench(exec_path: str, bench_args: List[str], xml_path: str) -> None:
    cmd = [exec_path] + bench_args + ["--reporter", f"xml::out={xml_path}"]
    result = subprocess.run(cmd)
    if result.returncode not in (0, 1, 4):
        sys.exit(result.returncode)


Benchmark = Tuple[str, float, float]
Row = Tuple[str, Optional[str], List[Benchmark]]
BenchmarkKey = Tuple[str, Optional[str], str]


def sort_rows(rows: List[Row]) -> List[Row]:
    out: List[Row] = []
    for tc_name, sec_name, benchmarks in rows:
        out.append((tc_name, sec_name, sorted(benchmarks, key=lambda b: b[0])))
    return sorted(out, key=lambda r: (r[0], "" if r[1] is None else r[1]))


def parse_xml(xml_path: str) -> List[Row]:
    if not os.path.getsize(xml_path):
        return []

    tree = ET.parse(xml_path)
    root = tree.getroot()
    rows: List[Row] = []

    for tc in root.findall("TestCase"):
        tc_name = tc.get("name", "")

        def extract(node: ET.Element) -> List[Benchmark]:
            out = []
            for br in node.findall("BenchmarkResults"):
                mean_el = br.find("mean")
                sd_el = br.find("standardDeviation")
                if mean_el is not None:
                    out.append(
                        (
                            br.get("name", ""),
                            float(mean_el.get("value", 0)),
                            float(sd_el.get("value", 0)) if sd_el is not None else 0.0,
                        )
                    )
            return out

        sections = tc.findall("Section")
        if sections:
            for sec in sections:
                benchmarks = extract(sec)
                if benchmarks:
                    rows.append((tc_name, sec.get("name"), benchmarks))
        else:
            benchmarks = extract(tc)
            if benchmarks:
                rows.append((tc_name, None, benchmarks))

    return rows


def print_report(rows: List[Row]) -> None:
    if not rows:
        print()
        print("No benchmark results.")
        return

    w_workload = 34
    w_variant = 32
    w_mean = 10
    w_sd = 10
    sep = "-" * (w_workload + w_variant + w_mean + w_sd + 8)

    print()
    print(f"{'WORKLOAD':<{w_workload}}  {'VARIANT':<{w_variant}}  {'MEAN':>{w_mean}}  {'+-SD':>{w_sd}}")
    print(sep)

    for tc_name, sec_name, benchmarks in rows:
        label = tc_name if sec_name is None else f"{tc_name} / {sec_name}"
        first = True
        for name, mean_ns, sd_ns in benchmarks:
            workload = label if first else ""
            first = False
            print(
                f"{workload:<{w_workload}}  {name:<{w_variant}}  "
                f"{fmt_time(mean_ns):>{w_mean}}  {fmt_time(sd_ns):>{w_sd}}"
            )


def fmt_delta(ratio: Optional[float]) -> str:
    if ratio is None:
        return ""
    return f"{ratio:+.1f}%"


def fmt_sigma(sigma: Optional[float]) -> str:
    if sigma is None:
        return ""
    if math.isinf(sigma):
        return "inf"
    return f"{sigma:.1f}"


def signal_label(sigma: Optional[float]) -> str:
    if sigma is None:
        return ""
    if sigma < 1.0:
        return "noise"
    if sigma < 2.0:
        return "weak"
    if sigma < 3.0:
        return "clear"
    return "strong"


def fmt_signal(sigma: Optional[float]) -> str:
    label = signal_label(sigma)
    if not label:
        return ""
    return f"{label} ({fmt_sigma(sigma)})"


def use_color() -> bool:
    return sys.stdout.isatty() and "NO_COLOR" not in os.environ and os.environ.get("TERM") != "dumb"


def signal_color(delta: Optional[float], signal: str) -> str:
    if signal == "noise" or delta is None or delta == 0.0:
        return DIM
    if delta > 0.0:
        if signal == "strong":
            return BOLD_RED
        if signal == "weak":
            return DIM + RED
        return RED

    if signal == "strong":
        return BOLD_GREEN
    if signal == "weak":
        return DIM + GREEN
    return GREEN


def signal_sigma(
    mean_ns: float,
    stddev_ns: float,
    previous_mean_ns: Optional[float],
    previous_stddev_ns: Optional[float],
) -> Optional[float]:
    if previous_mean_ns is None or previous_stddev_ns is None:
        return None

    delta_ns = abs(mean_ns - previous_mean_ns)
    noise_ns = math.hypot(stddev_ns, previous_stddev_ns)
    if noise_ns == 0.0:
        return 0.0 if delta_ns == 0.0 else math.inf
    return delta_ns / noise_ns


def git_value(args: List[str]) -> Optional[str]:
    result = subprocess.run(["git"] + args, text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def git_metadata() -> Dict[str, Any]:
    status = git_value(["status", "--porcelain"])
    commit = git_value(["rev-parse", "HEAD"])
    return {
        "commit": commit,
        "short_commit": git_value(["rev-parse", "--short", "HEAD"]),
        "branch": git_value(["branch", "--show-current"]),
        "dirty": bool(status),
    }


def flatten_rows(rows: List[Row]) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    for tc_name, sec_name, benchmarks in rows:
        for name, mean_ns, sd_ns in benchmarks:
            out.append(
                {
                    "workload": tc_name,
                    "section": sec_name,
                    "variant": name,
                    "mean_ns": mean_ns,
                    "stddev_ns": sd_ns,
                }
            )
    return out


def benchmark_key(benchmark: Dict[str, Any]) -> BenchmarkKey:
    return (
        str(benchmark.get("workload", "")),
        benchmark.get("section"),
        str(benchmark.get("variant", "")),
    )


def load_json_runs(path: str) -> List[Dict[str, Any]]:
    if not os.path.exists(path) or not os.path.getsize(path):
        return []

    with open(path, "r", encoding="utf-8") as f:
        payload = json.load(f)

    if isinstance(payload, dict) and isinstance(payload.get("runs"), list):
        return payload["runs"]
    if isinstance(payload, dict) and isinstance(payload.get("benchmarks"), list):
        return [payload]
    if isinstance(payload, list):
        return payload
    sys.exit(f"error: {path} is not a bench JSON run or history file")


def latest_by_key(runs: List[Dict[str, Any]]) -> Dict[BenchmarkKey, Dict[str, Any]]:
    latest: Dict[BenchmarkKey, Dict[str, Any]] = {}
    for run in runs:
        for benchmark in run.get("benchmarks", []):
            if isinstance(benchmark, dict):
                latest[benchmark_key(benchmark)] = benchmark
    return latest


def print_comparison(current_run: Dict[str, Any], history_path: str) -> None:
    history = latest_by_key(load_json_runs(history_path))
    current = current_run.get("benchmarks", [])
    rows: List[Tuple[str, Optional[str], str, float, Optional[float], Optional[float], Optional[float]]] = []

    for benchmark in current:
        if not isinstance(benchmark, dict):
            continue
        key = benchmark_key(benchmark)
        mean_ns = float(benchmark.get("mean_ns", 0.0))
        stddev_ns = float(benchmark.get("stddev_ns", 0.0))
        previous = history.get(key)
        previous_ns = float(previous.get("mean_ns", 0.0)) if previous else None
        previous_stddev_ns = float(previous.get("stddev_ns", 0.0)) if previous else None
        delta = ((mean_ns - previous_ns) / previous_ns * 100.0) if previous_ns else None
        sigma = signal_sigma(mean_ns, stddev_ns, previous_ns, previous_stddev_ns)
        rows.append((key[0], key[1], key[2], mean_ns, previous_ns, delta, sigma))

    if not rows:
        return

    w_workload = 34
    w_variant = 32
    w_mean = 10
    w_prev = 10
    w_delta = 8
    w_signal = 16
    sep = "-" * (w_workload + w_variant + w_mean + w_prev + w_delta + w_signal + 12)
    color_enabled = use_color()

    print()
    print(f"Compared with: {history_path}")
    print(
        f"{'WORKLOAD':<{w_workload}}  {'VARIANT':<{w_variant}}  "
        f"{'MEAN':>{w_mean}}  {'PREV':>{w_prev}}  {'DELTA':>{w_delta}}  {'SIGNAL':>{w_signal}}"
    )
    print(sep)

    for workload, section, variant, mean_ns, previous_ns, delta, sigma in rows:
        label = workload if section is None else f"{workload} / {section}"
        prev = fmt_time(previous_ns) if previous_ns is not None else ""
        signal = signal_label(sigma)
        line = (
            f"{label:<{w_workload}}  {variant:<{w_variant}}  "
            f"{fmt_time(mean_ns):>{w_mean}}  {prev:>{w_prev}}  "
            f"{fmt_delta(delta):>{w_delta}}  {fmt_signal(sigma):>{w_signal}}"
        )
        color = signal_color(delta, signal) if color_enabled else ""
        print(f"{color}{line}{RESET if color else ''}")


def build_json_run(exec_path: str, bench_args: List[str], rows: List[Row]) -> Dict[str, Any]:
    return {
        "schema_version": 1,
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat().replace("+00:00", "Z"),
        "executable": exec_path,
        "args": bench_args,
        "git": git_metadata(),
        "benchmarks": flatten_rows(rows),
    }


def write_json(path: str, payload: Dict[str, Any]) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, sort_keys=True)
        f.write("\n")


def append_json(path: str, run: Dict[str, Any]) -> None:
    payload: Dict[str, Any]
    if os.path.exists(path) and os.path.getsize(path):
        with open(path, "r", encoding="utf-8") as f:
            existing = json.load(f)
        if isinstance(existing, dict) and isinstance(existing.get("runs"), list):
            payload = existing
        elif isinstance(existing, list):
            payload = {"schema_version": 1, "runs": existing}
        else:
            sys.exit(f"error: {path} is not a bench history JSON file")
    else:
        payload = {"schema_version": 1, "runs": []}

    payload["runs"].append(run)
    write_json(path, payload)


def main() -> int:
    exec_path, json_out, json_append, compare_path, bench_args = split_args(sys.argv[1:])
    resolved = resolve_exec(exec_path)
    if resolved is None:
        sys.exit(
            f"error: bench-archimedes not found at '{exec_path}'\n"
            f"  Build it first: cmake --build build --target bench-archimedes\n"
            f"  Or specify:    python3 bench/report.py --exec <path>"
        )

    with tempfile.NamedTemporaryFile(suffix=".xml", delete=False) as tmp:
        xml_path = tmp.name

    try:
        run_bench(resolved, bench_args, xml_path)
        rows = sort_rows(parse_xml(xml_path))
        print_report(rows)
        if json_out or json_append or compare_path:
            run = build_json_run(resolved, bench_args, rows)
            if compare_path:
                print_comparison(run, compare_path)
            if json_out:
                write_json(json_out, run)
            if json_append:
                append_json(json_append, run)
    finally:
        try:
            os.unlink(xml_path)
        except OSError:
            pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
