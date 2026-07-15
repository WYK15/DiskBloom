# High-Performance Scan Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a cancellable native Windows filesystem scanner backed by a compact contiguous tree, bounded progress publication, correctness tests, and Release benchmarks.

**Architecture:** The platform enumerator calls Win32 directory APIs and appends nodes and names to contiguous stores owned by `ScanTree`. A `ScanSession` runs one scan on a `std::jthread`, publishes only atomic counters while active, and transfers the completed tree after the worker stops mutating it. The UI never observes a partially mutable tree.

**Tech Stack:** C++20, Win32 `FindFirstFileExW`/`FindNextFileW`, `std::jthread`, `std::stop_token`, CTest, CMake.

## Global Constraints

- No `std::filesystem` in the scan hot path.
- No heap allocation per file node; nodes and names append to amortized contiguous stores.
- Reparse-point directories are recorded but not traversed in V1.
- Cancellation is checked during every enumeration loop.
- Progress publication is bounded and never posts one UI event per entry.
- All counters and byte totals use explicit fixed-width types.
- Debug and x64 Release tests must pass before UI integration.

---

### Task 1: Compact Scan Tree

**Files:**
- Create: `src/core/scan_tree.h`
- Create: `src/core/scan_tree.cpp`
- Create: `tests/scan_tree_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `constexpr NodeIndex invalid_node`.
- Produces: `enum class ScanNodeFlags : uint16_t { None, Directory, ReparsePoint, Inaccessible, Incomplete }` plus bit operators.
- Produces: `struct ScanNode` with parent/firstChild/nextSibling/nameOffset/nameLength/logicalSize/fileCount/directoryCount/flags.
- Produces: `class ScanTree` with `reserve`, `add_root`, `add_child`, `name`, `node`, `nodes`, `aggregate`, and `estimated_memory_bytes`.

- [ ] **Step 1: Write failing tests**

```cpp
ScanTree tree;
const auto root = tree.add_root(L"C:", ScanNodeFlags::Directory);
const auto folder = tree.add_child(root, L"folder", 0, ScanNodeFlags::Directory);
tree.add_child(folder, L"a.bin", 10, ScanNodeFlags::None);
tree.add_child(folder, L"b.bin", 20, ScanNodeFlags::None);
tree.aggregate();
CHECK(tree.node(root).logicalSize == 30);
CHECK(tree.node(root).fileCount == 2);
CHECK(tree.node(root).directoryCount == 1);
```

- [ ] **Step 2: Build and confirm failure because `ScanTree` is missing**

Run: `cmake --build --preset windows-debug`

- [ ] **Step 3: Implement contiguous nodes and UTF-16 name arena**

Use `std::vector<ScanNode>` and `std::vector<wchar_t>`. Parent/child relationships use indices. `aggregate` walks nodes in reverse insertion order and adds each node's completed totals to its parent.

- [ ] **Step 4: Run all tests**

Run: `ctest --preset windows-debug --output-on-failure`

- [ ] **Step 5: Commit**

```powershell
git add src/core/scan_tree.* tests/scan_tree_tests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add compact scan tree"
```

### Task 2: Win32 Directory Enumerator

**Files:**
- Create: `src/platform/windows/file_scanner.h`
- Create: `src/platform/windows/file_scanner.cpp`
- Create: `tests/file_scanner_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `struct ScanProgress { uint64_t entries, files, directories, logicalBytes, errors; }`.
- Produces: `enum class ScanCompletion { Completed, Cancelled, RootUnavailable }`.
- Produces: `struct ScanResult { ScanCompletion completion; ScanProgress progress; ScanTree tree; }`.
- Produces: `using ProgressCallback = std::function<void(const ScanProgress&)>`.
- Produces: `ScanResult scan_directory(std::wstring_view rootPath, std::stop_token, ProgressCallback)`.

- [ ] **Step 1: Write failing temporary-tree integration tests**

Create a unique directory below `%TEMP%`, write nested files of known byte sizes, scan it, and assert exact file/directory counts and root aggregate size. Add a pre-cancelled token test that returns `Cancelled` without traversing children.

- [ ] **Step 2: Build and confirm missing scanner API failure**

Run: `cmake --build --preset windows-debug`

- [ ] **Step 3: Implement iterative Win32 enumeration**

Use a vector of directory work items, `FindFirstFileExW` with `FindExInfoBasic` and `FIND_FIRST_EX_LARGE_FETCH`, fallback without the flag on `ERROR_INVALID_PARAMETER`, and `FindNextFileW`. Skip dot entries, retain reparse points as leaves, and normalize long absolute paths for API use.

- [ ] **Step 4: Bound progress callbacks**

Publish on completion, cancellation, or when either 4096 additional entries or 50 ms has elapsed. The callback receives counters only.

- [ ] **Step 5: Run Debug and Release correctness tests**

Run:

```powershell
ctest --preset windows-debug --output-on-failure
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

- [ ] **Step 6: Commit**

```powershell
git add src/platform/windows/file_scanner.* tests/file_scanner_tests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: scan directories with native Win32 APIs"
```

### Task 3: Asynchronous Scan Session

**Files:**
- Create: `src/core/scan_session.h`
- Create: `src/core/scan_session.cpp`
- Create: `tests/scan_session_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class ScanSessionState { Idle, Running, Cancelling, Completed, Cancelled, Failed }`.
- Produces: `class ScanSession` with `start`, `cancel`, `state`, `progress`, and `take_result`.

- [ ] **Step 1: Write failing state-transition and cancellation tests**

Use a temporary tree and poll with a bounded timeout. Assert `Idle -> Running -> Completed`, single-result transfer, and idempotent cancellation.

- [ ] **Step 2: Build and confirm missing session API failure**

- [ ] **Step 3: Implement `std::jthread` ownership and atomic progress**

The worker exclusively owns the mutable `ScanResult`. It stores the final result under one mutex only after enumeration and aggregation complete. `take_result` succeeds once after a terminal state.

- [ ] **Step 4: Run all tests under Debug and Release**

- [ ] **Step 5: Commit**

```powershell
git add src/core/scan_session.* tests/scan_session_tests.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add asynchronous cancellable scan session"
```

### Task 4: Scan Benchmarks

**Files:**
- Create: `benchmarks/CMakeLists.txt`
- Create: `benchmarks/scan_tree_benchmark.cpp`
- Create: `docs/qa/scan-engine.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `diskbloom_scan_tree_benchmark` executable accepting a synthetic node count.

- [ ] **Step 1: Generate a deterministic one-million-entry tree in Release**

The benchmark reserves stores, appends deterministic names and sizes, aggregates once, and prints elapsed time, nodes/second, and `estimated_memory_bytes`.

- [ ] **Step 2: Run the benchmark three times**

Run: `build/windows-release/benchmarks/diskbloom_scan_tree_benchmark.exe 1000000`

- [ ] **Step 3: Record raw results and machine details**

Write every run to `docs/qa/scan-engine.md`. Set regression gates from measured values; do not invent unmeasured thresholds.

- [ ] **Step 4: Commit**

```powershell
git add benchmarks CMakeLists.txt docs/qa/scan-engine.md
git commit -m "perf: establish scan engine baseline"
```

### Task 5: Disk Overview Scan State

**Files:**
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `src/app/disk_overview.h`
- Modify: `src/app/disk_overview.cpp`
- Create: `tests/scan_ui_state_tests.cpp`

**Interfaces:**
- Consumes: `ScanSession` and volume root paths.
- Produces: per-volume idle/running/cancelling/completed state, bounded progress repainting, and scan/cancel commands.

- [ ] **Step 1: Write failing UI-state reducer tests**

Assert idle scan click starts the selected root, running click requests cancellation, and terminal state exposes the completed result without blocking the UI thread.

- [ ] **Step 2: Implement a 33 ms window timer that polls atomic snapshots**

Start the timer only while a scan is active and stop it on terminal state. The timer invalidates only when visible counters or state changed.

- [ ] **Step 3: Render localized scan/cancel states**

Use `StringId::Scan`, `StringId::Cancel`, and existing semantic theme tokens. Do not add a per-entry window message.

- [ ] **Step 4: Verify Debug, Release, cancellation, both themes, and both languages**

- [ ] **Step 5: Commit**

```powershell
git add src/app tests/scan_ui_state_tests.cpp
git commit -m "feat: connect volume scanning to disk overview"
```

