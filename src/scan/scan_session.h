#pragma once

#include "platform/windows/file_scanner.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace diskbloom::scan {

enum class ScanSessionState {
    Idle,
    Running,
    Cancelling,
    Completed,
    Cancelled,
    Failed,
};

using ScanFunction = std::function<platform::windows::ScanResult(
    std::wstring_view,
    std::stop_token,
    platform::windows::ProgressCallback)>;

class ScanSession final {
public:
    explicit ScanSession(ScanFunction scanFunction = platform::windows::scan_directory);
    ~ScanSession();

    ScanSession(const ScanSession&) = delete;
    ScanSession& operator=(const ScanSession&) = delete;
    ScanSession(ScanSession&&) = delete;
    ScanSession& operator=(ScanSession&&) = delete;

    [[nodiscard]] bool start(std::wstring rootPath);
    void cancel() noexcept;

    [[nodiscard]] ScanSessionState state() const noexcept;
    [[nodiscard]] platform::windows::ScanProgress progress() const noexcept;
    [[nodiscard]] std::optional<platform::windows::ScanResult> take_result();

private:
    void publish_progress(const platform::windows::ScanProgress& progress) noexcept;
    void run(std::wstring rootPath, std::stop_token stopToken) noexcept;

    ScanFunction scanFunction_;
    std::atomic<ScanSessionState> state_{ScanSessionState::Idle};
    std::atomic<std::uint64_t> entries_{0U};
    std::atomic<std::uint64_t> files_{0U};
    std::atomic<std::uint64_t> directories_{0U};
    std::atomic<std::uint64_t> logicalBytes_{0U};
    std::atomic<std::uint64_t> errors_{0U};
    std::mutex resultMutex_;
    std::optional<platform::windows::ScanResult> result_;
    std::jthread worker_;
};

} // namespace diskbloom::scan
