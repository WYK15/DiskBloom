# Directory Transition Preference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing 700 ms DaisyDisk-style directory transition visible by default and expose persistent `Always on`, `Follow Windows`, and `Off` choices in Settings.

**Architecture:** `AppearanceSettings` owns a strongly typed animation preference and a pure resolver. A focused Windows settings store loads and atomically saves only that preference under Local AppData. `MainWindow` builds the localized submenu, resolves the policy at navigation time, and stops an active timer when the chosen policy becomes disabled.

**Tech Stack:** C++20, Win32 menus and filesystem APIs, `SPI_GETCLIENTAREAANIMATION`, Direct2D transition renderer, CMake/CTest, MSVC Debug and Release.

## Global Constraints

- `AlwaysOn` is the startup fallback when no valid saved value exists.
- `FollowSystem` is the only mode that consults `SPI_GETCLIENTAREAANIMATION`.
- `Off` completes navigation immediately and stops an active transition.
- Chart, breadcrumb, parent, back, and forward navigation use the same resolved policy.
- The existing 700 ms interpolation, 2,048-segment cap, 16 ms cadence, and 8 ms P95 benchmark gate remain unchanged.
- All new user-facing text is present in English and Simplified Chinese.
- The native Settings menu works in both dark and light application themes.
- Persistence runs only at startup and after an explicit settings command, never on the render or navigation hot path.
- Every behavioral change is developed test-first with an observed relevant failure.

---

### Task 1: Animation Preference Model

**Files:**
- Modify: `src/app/appearance_settings.h`
- Modify: `src/app/appearance_settings.cpp`
- Modify: `tests/appearance_settings_tests.cpp`

**Interfaces:**
- Produces: `DirectoryTransitionMode`, `resolve_directory_transitions`, three `SettingsCommand` values, and three QA launch arguments.
- Consumes: the existing `AppearanceSettings`, `apply_settings_command`, and `apply_launch_argument` flow.

- [x] **Step 1: Write failing model and resolver tests**

Add tests that establish the default and full truth table:

```cpp
TEST_CASE(directory_transitions_default_to_always_on) {
    AppearanceSettings settings{ThemeMode::System, Language::English};
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::AlwaysOn);
}

TEST_CASE(directory_transition_policy_resolves_all_modes) {
    CHECK(resolve_directory_transitions(DirectoryTransitionMode::AlwaysOn, false));
    CHECK(resolve_directory_transitions(DirectoryTransitionMode::AlwaysOn, true));
    CHECK(!resolve_directory_transitions(DirectoryTransitionMode::FollowSystem, false));
    CHECK(resolve_directory_transitions(DirectoryTransitionMode::FollowSystem, true));
    CHECK(!resolve_directory_transitions(DirectoryTransitionMode::Off, false));
    CHECK(!resolve_directory_transitions(DirectoryTransitionMode::Off, true));
}
```

Add command-isolation assertions for `DirectoryTransitionsAlwaysOn`, `DirectoryTransitionsFollowSystem`, and `DirectoryTransitionsOff`, plus launch-argument assertions for `--directory-transitions=always`, `system`, and `off`.

- [x] **Step 2: Build and verify RED**

Run in an x64 Visual Studio developer environment:

```powershell
cmake --build build/windows-debug --config Debug --target diskbloom_tests --parallel
```

Expected: compilation fails because `DirectoryTransitionMode` and `resolve_directory_transitions` are undefined.

- [x] **Step 3: Implement the minimal preference model**

Add these declarations and retain aggregate compatibility through a default member initializer:

```cpp
enum class DirectoryTransitionMode : std::uint8_t {
    AlwaysOn,
    FollowSystem,
    Off,
};

enum class SettingsCommand : int {
    ThemeSystem = 1001,
    ThemeLight = 1002,
    ThemeDark = 1003,
    LanguageEnglish = 1011,
    LanguageChinese = 1012,
    DirectoryTransitionsAlwaysOn = 1021,
    DirectoryTransitionsFollowSystem = 1022,
    DirectoryTransitionsOff = 1023,
};

struct AppearanceSettings {
    core::ThemeMode themeMode;
    core::Language language;
    DirectoryTransitionMode directoryTransitions = DirectoryTransitionMode::AlwaysOn;
};

[[nodiscard]] bool resolve_directory_transitions(
    DirectoryTransitionMode mode,
    bool windowsAnimationsEnabled) noexcept;
```

Implement the resolver as an exhaustive `switch`, extend both command and launch-argument parsers, and return `false` for unknown enum values or arguments.

- [x] **Step 4: Rebuild and verify GREEN**

Run:

```powershell
cmake --build build/windows-debug --config Debug --target diskbloom_tests --parallel
ctest --test-dir build/windows-debug -C Debug -R '^diskbloom_tests$' --output-on-failure
```

Expected: build succeeds and `diskbloom_tests` passes.

- [x] **Step 5: Commit the model**

```powershell
git add src/app/appearance_settings.h src/app/appearance_settings.cpp tests/appearance_settings_tests.cpp
git commit -m "feat: add directory transition preference model"
```

---

### Task 2: Atomic Preference Persistence

**Files:**
- Create: `src/platform/windows/settings_store.h`
- Create: `src/platform/windows/settings_store.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/settings_store_tests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `DirectoryTransitionMode` from Task 1 and an optional explicit path for deterministic tests.
- Produces: `default_settings_path`, `load_directory_transition_mode`, and `save_directory_transition_mode_atomic`.

- [ ] **Step 1: Write failing codec and filesystem tests**

Create a temporary test directory and cover exact valid values, missing files, malformed values, and preservation of the prior file when the sibling `.tmp` path cannot be opened:

```cpp
TEST_CASE(settings_store_round_trips_directory_transition_mode) {
    const auto path = unique_settings_test_path();
    CHECK(save_directory_transition_mode_atomic(path, DirectoryTransitionMode::FollowSystem));
    REQUIRE(load_directory_transition_mode(path).has_value());
    CHECK(*load_directory_transition_mode(path) == DirectoryTransitionMode::FollowSystem);
    cleanup_settings_test_path(path);
}

TEST_CASE(settings_store_failed_temp_write_preserves_prior_value) {
    const auto path = unique_settings_test_path();
    REQUIRE(save_directory_transition_mode_atomic(path, DirectoryTransitionMode::AlwaysOn));
    REQUIRE(CreateDirectoryW((path.wstring() + L".tmp").c_str(), nullptr));
    CHECK(!save_directory_transition_mode_atomic(path, DirectoryTransitionMode::Off));
    CHECK(load_directory_transition_mode(path) == DirectoryTransitionMode::AlwaysOn);
    cleanup_settings_test_path(path);
}
```

- [ ] **Step 2: Build and verify RED**

Run:

```powershell
cmake --build build/windows-debug --config Debug --target diskbloom_tests --parallel
```

Expected: compilation fails because `platform/windows/settings_store.h` does not exist.

- [ ] **Step 3: Implement the settings store**

Declare:

```cpp
[[nodiscard]] std::filesystem::path default_settings_path();
[[nodiscard]] std::optional<app::DirectoryTransitionMode>
load_directory_transition_mode(const std::filesystem::path& path) noexcept;
[[nodiscard]] bool save_directory_transition_mode_atomic(
    const std::filesystem::path& path,
    app::DirectoryTransitionMode mode) noexcept;
```

Use the exact UTF-8 file representation below:

```text
DiskBloomSettings/1
directoryTransitions=always
```

Accept only `always`, `system`, and `off`. Resolve the default path with `SHGetKnownFolderPath(FOLDERID_LocalAppData)` and append `DiskBloom/settings-v1.ini`. Save by creating the parent directory, writing and flushing `<path>.tmp`, closing it, then calling `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`. Delete only the temporary file after failure; never delete or truncate the prior destination.

- [ ] **Step 4: Run focused persistence tests**

Run:

```powershell
cmake --build build/windows-debug --config Debug --target diskbloom_tests --parallel
ctest --test-dir build/windows-debug -C Debug -R '^diskbloom_tests$' --output-on-failure
```

Expected: valid modes round-trip, invalid/missing data returns `std::nullopt`, and the prior valid destination survives the forced temp-write failure.

- [ ] **Step 5: Commit persistence**

```powershell
git add src/platform/windows/settings_store.h src/platform/windows/settings_store.cpp src/CMakeLists.txt tests/settings_store_tests.cpp tests/CMakeLists.txt
git commit -m "feat: persist directory transition preference"
```

---

### Task 3: Localized Settings Menu And Startup Loading

**Files:**
- Modify: `src/core/language.h`
- Modify: `src/core/string_catalog.cpp`
- Modify: `src/app/win_main.cpp`
- Modify: `src/app/main_window.cpp`
- Modify: `tests/string_catalog_tests.cpp`
- Modify: `tests/appearance_settings_tests.cpp`

**Interfaces:**
- Consumes: preference model and settings-store APIs from Tasks 1-2.
- Produces: localized menu entries and startup/saving integration.

- [ ] **Step 1: Write failing localization and startup-default tests**

Add `StringId` coverage for:

```cpp
CHECK(get_string(Language::English, StringId::DirectoryTransitions)
    == L"Directory transitions");
CHECK(get_string(Language::SimplifiedChinese, StringId::DirectoryTransitions)
    == L"\u76ee\u5f55\u5207\u6362\u52a8\u753b");
CHECK(get_string(Language::English, StringId::AnimationsAlwaysOn) == L"Always on");
CHECK(get_string(Language::English, StringId::AnimationsFollowWindows) == L"Follow Windows");
CHECK(get_string(Language::English, StringId::AnimationsOff) == L"Off");
```

Extend the catalog completeness loop so every new ID is non-empty in both languages.

- [ ] **Step 2: Build and verify RED**

Run the focused unit-test build and observe missing `StringId` compilation failures.

- [ ] **Step 3: Add catalog entries and menu construction**

Append these IDs before `Count`:

```cpp
DirectoryTransitions,
AnimationsAlwaysOn,
AnimationsFollowWindows,
AnimationsOff,
```

Add English and escaped Simplified Chinese strings. In `show_settings_menu`, create a third popup menu, append the three checked commands, attach it under the localized `DirectoryTransitions` label, and destroy the root menu as today. If any popup allocation fails, destroy every successfully created menu and return.

- [ ] **Step 4: Load before QA overrides and save explicit menu changes**

In `wWinMain`, initialize `AlwaysOn`, then load:

```cpp
const auto settingsPath = diskbloom::platform::windows::default_settings_path();
if (const auto saved = diskbloom::platform::windows::load_directory_transition_mode(settingsPath)) {
    appearance.directoryTransitions = *saved;
}
```

Parse command-line overrides afterward so QA arguments win without rewriting disk state. After `apply_settings_command` succeeds in `show_settings_menu`, call `save_directory_transition_mode_atomic` only when the selected command is one of the three directory-transition commands.

- [ ] **Step 5: Build and run catalog plus smoke coverage**

Run:

```powershell
cmake --build build/windows-debug --config Debug --target diskbloom_tests DiskBloom --parallel
ctest --test-dir build/windows-debug -C Debug -R 'diskbloom_tests|diskbloom_app_smoke_' --output-on-failure
```

Expected: catalogs and settings commands pass; all four dark/light and English/Chinese smoke tests pass.

- [ ] **Step 6: Commit menu and startup integration**

```powershell
git add src/core/language.h src/core/string_catalog.cpp src/app/win_main.cpp src/app/main_window.cpp tests/string_catalog_tests.cpp tests/appearance_settings_tests.cpp
git commit -m "feat: expose directory transition settings"
```

---

### Task 4: Runtime Animation Policy

**Files:**
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Create: `src/app/directory_transition_policy.h`
- Create: `src/app/directory_transition_policy.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/directory_transition_policy_tests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: selected `DirectoryTransitionMode`, Windows animation flag, and whether an analyzer transition is active.
- Produces: one effective policy for navigation and a testable action describing whether an active transition/timer must stop after settings or system changes.

- [ ] **Step 1: Write failing runtime-policy tests**

Define expected state transitions without a native window:

```cpp
TEST_CASE(turning_directory_transitions_off_stops_active_animation) {
    const auto action = compute_directory_transition_policy_change(
        DirectoryTransitionMode::Off, true, true);
    CHECK(!action.animationsEnabled);
    CHECK(action.completeActiveTransition);
}

TEST_CASE(system_change_only_stops_follow_system_animation) {
    CHECK(compute_directory_transition_policy_change(
        DirectoryTransitionMode::AlwaysOn, false, true).animationsEnabled);
    const auto follow = compute_directory_transition_policy_change(
        DirectoryTransitionMode::FollowSystem, false, true);
    CHECK(!follow.animationsEnabled);
    CHECK(follow.completeActiveTransition);
}
```

- [ ] **Step 2: Build and verify RED**

Run the unit-test target and observe the missing policy header failure.

- [ ] **Step 3: Implement and integrate the policy**

Expose:

```cpp
struct DirectoryTransitionPolicyChange {
    bool animationsEnabled = true;
    bool completeActiveTransition = false;
};

[[nodiscard]] DirectoryTransitionPolicyChange compute_directory_transition_policy_change(
    DirectoryTransitionMode mode,
    bool windowsAnimationsEnabled,
    bool transitionActive) noexcept;
```

Add a `MainWindow::directory_transitions_enabled()` helper that calls the pure resolver with the current system flag. Replace the direct `client_area_animations_enabled()` argument in `navigate_to_root`. After a transition setting command and on `WM_SETTINGCHANGE`, compute the policy; when `completeActiveTransition` is true, call `analyzer_.cancel_transition()`, kill `animation_timer_id`, and invalidate so the destination frame is drawn immediately.

- [ ] **Step 4: Run unit and render tests**

Run:

```powershell
cmake --build build/windows-debug --config Debug --parallel
ctest --test-dir build/windows-debug -C Debug --output-on-failure
```

Expected: the full Debug suite passes, including all theme/language smoke tests and the analyzer transition capture sequence.

- [ ] **Step 5: Commit runtime integration**

```powershell
git add src/app/directory_transition_policy.h src/app/directory_transition_policy.cpp src/app/main_window.h src/app/main_window.cpp src/CMakeLists.txt tests/directory_transition_policy_tests.cpp tests/CMakeLists.txt
git commit -m "fix: honor explicit directory animation policy"
```

---

### Task 5: Release Verification And Evidence

**Files:**
- Modify: `docs/qa/sunburst-analyzer.md`
- Modify: `docs/superpowers/plans/2026-07-16-directory-transition-preference.md`

**Interfaces:**
- Consumes: completed Tasks 1-4.
- Produces: checked plan, repeatable verification record, latest Release executable, and GitHub-ready history.

- [ ] **Step 1: Build Release in the MSVC environment**

Run:

```powershell
cmake --build build/windows-release --config Release --parallel
```

Expected: `build/windows-release/src/DiskBloom.exe` is relinked successfully.

- [ ] **Step 2: Run the complete Release suite**

Run:

```powershell
ctest --test-dir build/windows-release -C Release --output-on-failure
```

Expected: all unit, rendering, dark/light, English/Chinese, persistence, collector, and benchmark tests pass.

- [ ] **Step 3: Verify the target-machine regression**

With `SPI_GETCLIENTAREAANIMATION` returning false, launch the Release executable with:

```powershell
build/windows-release/src/DiskBloom.exe --directory-transitions=always
build/windows-release/src/DiskBloom.exe --directory-transitions=system
build/windows-release/src/DiskBloom.exe --directory-transitions=off
```

Expected: `always` visibly animates directory entry; `system` and `off` navigate immediately. Repeat the menu check in dark/light and English/Chinese.

- [ ] **Step 4: Record verification and check the plan**

Document the resolved root cause, three-mode behavior, Release test count, benchmark result, and executable path in `docs/qa/sunburst-analyzer.md`. Mark every completed plan checkbox, run `git diff --check`, and verify no generated settings file is tracked.

- [ ] **Step 5: Commit QA evidence**

```powershell
git add docs/qa/sunburst-analyzer.md docs/superpowers/plans/2026-07-16-directory-transition-preference.md
git commit -m "docs: verify directory transition preference"
```
