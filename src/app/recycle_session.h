#pragma once

#include "platform/windows/recycle_bin.h"

#include <Windows.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace diskbloom::app {

enum class RecycleSessionState {
    Idle,
    Running,
    Succeeded,
    Cancelled,
    Failed,
};

using RecycleFunction = std::function<platform::windows::RecycleResult(
    std::span<const std::wstring>,
    HWND)>;

class RecycleSession final {
public:
    explicit RecycleSession(RecycleFunction recycleFunction = platform::windows::recycle_paths);
    ~RecycleSession();

    RecycleSession(const RecycleSession&) = delete;
    RecycleSession& operator=(const RecycleSession&) = delete;

    [[nodiscard]] bool start(std::vector<std::wstring> paths, HWND ownerWindow);
    [[nodiscard]] RecycleSessionState state() const noexcept;
    [[nodiscard]] std::optional<platform::windows::RecycleResult> take_result();

private:
    void run(std::vector<std::wstring> paths, HWND ownerWindow) noexcept;

    RecycleFunction recycleFunction_;
    std::atomic<RecycleSessionState> state_{RecycleSessionState::Idle};
    std::mutex resultMutex_;
    std::optional<platform::windows::RecycleResult> result_;
    std::jthread worker_;
};

} // namespace diskbloom::app
