#include "app/recycle_session.h"

#include <utility>

namespace diskbloom::app {

RecycleSession::RecycleSession(RecycleFunction recycleFunction)
    : recycleFunction_(std::move(recycleFunction)) {}

RecycleSession::~RecycleSession() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool RecycleSession::start(std::vector<std::wstring> paths, const HWND ownerWindow) {
    if (paths.empty()) {
        return false;
    }
    auto expected = RecycleSessionState::Idle;
    if (!state_.compare_exchange_strong(
            expected,
            RecycleSessionState::Running,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return false;
    }

    try {
        worker_ = std::jthread(
            [this, paths = std::move(paths), ownerWindow](const std::stop_token) mutable {
                run(std::move(paths), ownerWindow);
            });
    } catch (...) {
        state_.store(RecycleSessionState::Failed, std::memory_order_release);
        throw;
    }
    return true;
}

RecycleSessionState RecycleSession::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

std::optional<platform::windows::RecycleResult> RecycleSession::take_result() {
    const auto current = state();
    if (current == RecycleSessionState::Idle || current == RecycleSessionState::Running) {
        return std::nullopt;
    }
    std::scoped_lock lock(resultMutex_);
    auto result = result_;
    result_.reset();
    return result;
}

void RecycleSession::run(
    std::vector<std::wstring> paths,
    const HWND ownerWindow) noexcept {
    auto result = platform::windows::RecycleResult::Failed;
    try {
        result = recycleFunction_(paths, ownerWindow);
    } catch (...) {
        result = platform::windows::RecycleResult::Failed;
    }
    {
        std::scoped_lock lock(resultMutex_);
        result_ = result;
    }

    switch (result) {
    case platform::windows::RecycleResult::Succeeded:
        state_.store(RecycleSessionState::Succeeded, std::memory_order_release);
        break;
    case platform::windows::RecycleResult::Cancelled:
        state_.store(RecycleSessionState::Cancelled, std::memory_order_release);
        break;
    case platform::windows::RecycleResult::InvalidPath:
    case platform::windows::RecycleResult::NotFound:
    case platform::windows::RecycleResult::Failed:
        state_.store(RecycleSessionState::Failed, std::memory_order_release);
        break;
    }
}

} // namespace diskbloom::app
