# Scan Engine Performance Baseline

Date: 2026-07-16

## Scope

This baseline measures the contiguous `ScanTree` append and reverse aggregation
paths. It deliberately excludes filesystem enumeration and storage latency. The
benchmark creates a deterministic one-million-node hierarchy, pre-reserves node
and UTF-16 name storage, aggregates once, and validates the root byte total.

## Build And Host

- Build: `windows-release` (`CMAKE_BUILD_TYPE=Release`)
- Compiler: Microsoft C/C++ Optimizing Compiler 19.51.36248 for x64
- CPU: AMD Ryzen 7 PRO 4750G, 8 cores / 16 logical processors
- RAM: 16,510,099,456 bytes
- OS: Windows 11 Pro for Workstations, 10.0.26100 (build 26100)

Command:

```powershell
build/windows-release/benchmarks/diskbloom_scan_tree_benchmark.exe 1000000
```

## Raw Results

| Run | Build (ms) | Aggregate (ms) | Total (ms) | Nodes/second | Estimated bytes | Bytes/node |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 26.38 | 10.37 | 36.75 | 27,214,364.83 | 77,996,098 | 78.00 |
| 2 | 28.53 | 10.38 | 38.92 | 25,696,635.80 | 77,996,098 | 78.00 |
| 3 | 26.88 | 9.22 | 36.10 | 27,701,751.86 | 77,996,098 | 78.00 |

Median total time: **36.75 ms**. Median throughput: **27,214,364.83
nodes/second**.

## Regression Gates

For the same host, compiler, Release preset, node count, and an otherwise idle
machine:

- The benchmark validation must pass.
- Estimated memory must not exceed the measured 77,996,098-byte baseline.
- The median of three runs must not exceed the measured 36.75 ms median.
- The median of three runs must not fall below the measured 27,214,364.83
  nodes/second median.

Timing gates are comparison signals, not portable absolute requirements. A new
machine or toolchain requires a separately recorded baseline before enforcing
timing regressions there.
