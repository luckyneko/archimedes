#!/usr/bin/env python3
"""
Run bench-archimedes and print a compact Catch2 benchmark summary.

Usage:
    python3 bench/report.py [--exec <path>] [bench-archimedes args...]

Examples:
    python3 bench/report.py "[fast]"
    python3 bench/report.py "[bench]" --benchmark-samples=30
    python3 bench/report.py --exec build-release/bench-archimedes "[bench]"
"""

import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from typing import List, Optional, Tuple

DEFAULT_EXEC = "./build/bench-archimedes"


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


def split_args(argv: List[str]) -> Tuple[str, List[str]]:
    exec_path = DEFAULT_EXEC
    rest: List[str] = []
    i = 0
    while i < len(argv):
        if argv[i] == "--exec":
            if i + 1 >= len(argv):
                sys.exit("error: --exec requires a path argument")
            exec_path = argv[i + 1]
            i += 2
        else:
            rest.append(argv[i])
            i += 1
    return exec_path, rest


def run_bench(exec_path: str, bench_args: List[str], xml_path: str) -> None:
    cmd = [exec_path] + bench_args + ["--reporter", f"xml::out={xml_path}"]
    result = subprocess.run(cmd)
    if result.returncode not in (0, 1, 4):
        sys.exit(result.returncode)


Benchmark = Tuple[str, float, float]
Row = Tuple[str, Optional[str], List[Benchmark]]


def parse_xml(xml_path: str) -> List[Row]:
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


def main() -> int:
    exec_path, bench_args = split_args(sys.argv[1:])
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
        print_report(parse_xml(xml_path))
    finally:
        try:
            os.unlink(xml_path)
        except OSError:
            pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
