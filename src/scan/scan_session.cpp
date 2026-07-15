#include "scan/scan_session.h"

#include <utility>

namespace diskbloom::scan {

ScanSession::ScanSession(ScanFunction scanFunction)
    : scanFunction_(std::move(scanFunction)) {}

ScanSession::~ScanSession() {
    cancel();
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
}

bool ScanSession::start(std::wstring rootPath) {
    auto expected = ScanSessionState::Idle;
    if (!state_.compare_exchange_strong(
            expected,
            ScanSessionState::Running,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return false;
    }

    try {
        worker_ = std::jthread(
            [this, rootPath = std::move(rootPath)](const std::stop_token stopToken) mutable {
                run(std::move(rootPath), stopToken);
            });
    } catch (...) {
        state_.store(ScanSessionState::Failed, std::memory_order_release);
        throw;
    }
    return true;
}

void ScanSession::cancel() noexcept {
    auto expected = ScanSessionState::Running;
    if (state_.compare_exchange_strong(
            expected,
            ScanSessionState::Cancelling,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        worker_.request_stop();
    }
}

ScanSessionState ScanSession::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

platform::windows::ScanProgress ScanSession::progress() const noexcept {
    platform::windows::ScanProgress snapshot{};
    snapshot.entries = entries_.load(std::memory_order_relaxed);
    snapshot.files = files_.load(std::memory_order_relaxed);
    snapshot.directories = directories_.load(std::memory_order_relaxed);
    snapshot.logicalBytes = logicalBytes_.load(std::memory_order_relaxed);
    snapshot.errors = errors_.load(std::memory_order_relaxed);
    return snapshot;
}

std::optional<platform::windows::ScanResult> ScanSession::take_result() {
    const auto currentState = state();
    if (currentState != ScanSessionState::Completed
        && currentState != ScanSessionState::Cancelled
        && currentState != ScanSessionState::Failed) {
        return std::nullopt;
    }

    std::scoped_lock lock(resultMutex_);
    auto result = std::move(result_);
    result_.reset();
    return result;
}

void ScanSession::publish_progress(const platform::windows::ScanProgress& progress) noexcept {
    entries_.store(progress.entries, std::memory_order_relaxed);
    files_.store(progress.files, std::memory_order_relaxed);
    directories_.store(progress.directories, std::memory_order_relaxed);
    logicalBytes_.store(progress.logicalBytes, std::memory_order_relaxed);
    errors_.store(progress.errors, std::memory_order_relaxed);
}

void ScanSession::run(std::wstring rootPath, const std::stop_token stopToken) noexcept {
    try {
        auto result = scanFunction_(
            rootPath,
            stopToken,
            [this](const platform::windows::ScanProgress& progress) {
                publish_progress(progress);
            });
        publish_progress(result.progress);

        const auto completion = result.completion;
        {
            std::scoped_lock lock(resultMutex_);
            result_.emplace(std::move(result));
        }

        switch (completion) {
        case platform::windows::ScanCompletion::Completed:
            state_.store(ScanSessionState::Completed, std::memory_order_release);
            break;
        case platform::windows::ScanCompletion::Cancelled:
            state_.store(ScanSessionState::Cancelled, std::memory_order_release);
            break;
        case platform::windows::ScanCompletion::RootUnavailable:
            state_.store(ScanSessionState::Failed, std::memory_order_release);
            break;
        }
    } catch (...) {
        state_.store(ScanSessionState::Failed, std::memory_order_release);
    }
}

} // namespace diskbloom::scan
