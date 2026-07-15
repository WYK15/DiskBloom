# File Actions And Deletion Review Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the DaisyDisk-style ranked child list, selected-item actions, explicit deletion review, Recycle Bin execution, and folder scanning without blocking the UI thread.

**Architecture:** A completed scan retains one normalized absolute root path alongside its immutable tree. Core path reconstruction and child ranking are platform-independent and bounded; Windows Shell adapters receive explicit absolute paths for preview, reveal, folder picking, and recycle-only deletion. The analyzer owns selection and review UI state but never deletes directly from a chart click.

**Tech Stack:** C++20, contiguous `ScanTree`, Win32 Shell APIs, `IFileOperation`, `IFileDialog`, Direct2D/DirectWrite, CTest.

## Global Constraints

- All new UI states support light/dark themes and zh-CN/en-US resources.
- File clicks select or navigate; they never delete immediately.
- Deletion requires an explicit collection/review step and a separate confirmation.
- Windows deletion uses the Recycle Bin and never silently falls back to permanent deletion.
- Shell calls that can block run outside paint and pointer handlers.
- Child lists are capped and do not allocate or sort the full million-node tree.

---

### Task 1: Absolute Paths And Ranked Child Model

**Files:**
- Modify: `src/platform/windows/file_scanner.h`
- Modify: `src/platform/windows/file_scanner.cpp`
- Create: `src/core/node_path.h`
- Create: `src/core/node_path.cpp`
- Create: `src/core/child_ranking.h`
- Create: `src/core/child_ranking.cpp`
- Create: `tests/node_path_tests.cpp`
- Create: `tests/child_ranking_tests.cpp`

**Interfaces:**
- `ScanResult` retains `std::wstring rootPath` once per scan.
- `build_node_path(const ScanTree&, NodeIndex, std::wstring_view rootPath)` returns an optional normalized display path.
- `rank_children(const ScanTree&, NodeIndex, size_t limit)` returns at most `limit` largest direct children using bounded memory.

- [ ] Write failing root/nested/invalid path tests and top-K ranking tests.
- [ ] Verify RED on the missing APIs.
- [ ] Implement ancestor collection plus one final path allocation and a fixed-size min-heap for top-K children.
- [ ] Run Debug/Release tests and commit `feat: add bounded child ranking and node paths`.

### Task 2: Analyzer Child List And Selection Actions

**Files:**
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/core/language.h`
- Modify: `src/core/string_catalog.cpp`
- Create: `tests/analyzer_child_list_tests.cpp`

**Interfaces:**
- The right panel consumes the top 24 ranked direct children and exposes row hit targets.
- File activation selects a file; directory activation drills down.
- Localized commands expose Open/Preview, Show in Explorer, and Add to Review.

- [ ] Write failing list layout, ranking order, and row/action hit tests.
- [ ] Verify RED.
- [ ] Render ranked rows with stable 32 DIP height, size column, selection/hover states, and bounded clipping.
- [ ] Run both themes/languages and commit `feat: add analyzer child list and actions`.

### Task 3: Windows Preview And Explorer Reveal

**Files:**
- Create: `src/platform/windows/shell_actions.h`
- Create: `src/platform/windows/shell_actions.cpp`
- Create: `tests/shell_actions_tests.cpp`
- Modify: `src/app/main_window.cpp`

**Interfaces:**
- `open_with_shell(path)` uses `ShellExecuteExW` for the system-associated preview/open behavior.
- `reveal_in_explorer(path)` uses `SHOpenFolderAndSelectItems` for files and Explorer for directories.

- [ ] Write failing argument/path validation tests.
- [ ] Verify RED.
- [ ] Implement narrow Shell PIDL ownership wrappers and structured results.
- [ ] Run integration checks on a temporary file/folder and commit `feat: preview and reveal analyzer items`.

### Task 4: Review Collection And Recycle-Only Deletion

**Files:**
- Create: `src/app/deletion_review.h`
- Create: `src/app/deletion_review.cpp`
- Create: `src/platform/windows/recycle_bin.h`
- Create: `src/platform/windows/recycle_bin.cpp`
- Create: `tests/deletion_review_tests.cpp`
- Modify: `src/app/analyzer_view.*`
- Modify: `src/app/main_window.*`

**Interfaces:**
- Review state stores unique node indices from the current immutable scan and computes total bytes.
- `recycle_paths` uses `IFileOperation` with `FOFX_RECYCLEONDELETE` and returns per-operation success/cancel/failure.

- [ ] Write failing uniqueness, ancestor-overlap, total-size, confirmation, and cancellation tests.
- [ ] Verify RED.
- [ ] Implement the bottom review band and localized confirmation dialog.
- [ ] Execute recycle-only deletion on a temporary fixture, rescan after success, and commit `feat: review and recycle selected items`.

### Task 5: Folder Picker And Scan Reuse

**Files:**
- Create: `src/platform/windows/folder_picker.h`
- Create: `src/platform/windows/folder_picker.cpp`
- Modify: `src/app/main_window.*`
- Modify: `src/app/scan_ui_state.*`
- Create: `tests/folder_scan_state_tests.cpp`

**Interfaces:**
- `pick_folder(HWND)` uses `IFileOpenDialog` with `FOS_PICKFOLDERS`.
- Folder scans reuse `ScanSession`, progress polling, cancellation, completed-result transfer, and analyzer navigation.

- [ ] Write failing folder-operation state tests.
- [ ] Verify RED.
- [ ] Add picker and a dedicated folder scan status without introducing a second scan implementation.
- [ ] Verify both languages/themes, cancellation, and analyzer entry; commit `feat: scan selected folders`.
