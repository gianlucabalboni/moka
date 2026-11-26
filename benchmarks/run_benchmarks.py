#!/usr/bin/env python3
import os
import statistics
import subprocess
import time

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(ROOT, "build")
os.makedirs(BUILD, exist_ok=True)

CASES = ["Startup", "FibCpu", "ListInt", "GcStress"]
RUNS = 3


def measure_cmd(cmd):
    times = []
    rss_list = []
    out_text = ""
    for _ in range(RUNS):
        t0 = time.perf_counter()
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        _, status, rusage = os.wait4(proc.pid, 0)
        t1 = time.perf_counter()
        stdout = proc.stdout.read().decode().strip()
        stderr = proc.stderr.read().decode().strip()
        proc.stdout.close()
        proc.stderr.close()
        if os.WIFEXITED(status) and os.WEXITSTATUS(status) != 0:
            raise RuntimeError(f"Command {cmd} failed: {stderr}")
        times.append((t1 - t0) * 1000.0)
        rss_list.append(rusage.ru_maxrss)  # KiB on Linux
        out_text = stdout
    return statistics.median(times), statistics.median(rss_list), out_text


def main():
    print("Compiling Moka and Java benchmarks...")
    for name in CASES:
        moka_src = os.path.join(ROOT, "moka", f"{name}.moka")
        moka_bin = os.path.join(BUILD, f"moka_{name}")
        subprocess.run(["mokac", moka_src, "-o", moka_bin], check=True)

        java_src = os.path.join(ROOT, "java", f"{name}.java")
        subprocess.run(["javac", "-d", BUILD, java_src], check=True)

    print(f"\nRunning {RUNS} iterations each (reporting median):\n")
    header = f"{'Benchmark':<12} | {'Moka Time':>10} | {'Java Time':>10} | {'Moka RSS':>10} | {'Java RSS':>10}"
    print(header)
    print("-" * len(header))

    for name in CASES:
        moka_bin = os.path.join(BUILD, f"moka_{name}")
        m_ms, m_rss, m_out = measure_cmd([moka_bin])
        j_ms, j_rss, j_out = measure_cmd(["java", "-cp", BUILD, name])
        print(
            f"{name:<12} | {m_ms:>8.1f}ms | {j_ms:>8.1f}ms | {m_rss/1024.0:>7.2f}MB | {j_rss/1024.0:>7.2f}MB"
        )


if __name__ == "__main__":
    main()
