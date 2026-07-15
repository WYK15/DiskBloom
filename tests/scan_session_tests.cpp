#include "test_support.h"

#include "scan/scan_session.h"

#include <chrono>
#include <thread>

using diskbloom::platform::windows::ScanCompletion;
using diskbloom::platform::windows::ScanResult;
using diskbloom::scan::ScanSession;
using diskbloom::scan::ScanSessionState;

namespace {

bool wait_for_terminal(ScanSession& session) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto state = session.state();
        if (state == ScanSessionState::Completed
            || state == ScanSessionState::Cancelled
            || state == ScanSessionState::Failed) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

} // namespace

TEST_CASE(scan_session_completes_and_transfers_result_once) {
    ScanSession session([](const auto&, const auto, const auto& callback) {
        diskbloom::platform::windows::ScanProgress progress{};
        progress.entries = 7U;
        progress.files = 5U;
        callback(progress);
        ScanResult result{};
        result.completion = ScanCompletion::Completed;
        result.progress = progress;
        return result;
    });

    CHECK(session.state() == ScanSessionState::Idle);
    CHECK(session.start(L"ignored"));
    CHECK(wait_for_terminal(session));
    CHECK(session.state() == ScanSessionState::Completed);
    CHECK(session.progress().entries == 7U);
    auto result = session.take_result();
    CHECK(result.has_value());
    CHECK(result->progress.files == 5U);
    CHECK(!session.take_result().has_value());
}

TEST_CASE(scan_session_cancellation_is_idempotent) {
    ScanSession session([](const auto&, const std::stop_token token, const auto& callback) {
        diskbloom::platform::windows::ScanProgress progress{};
        while (!token.stop_requested()) {
            ++progress.entries;
            callback(progress);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ScanResult result{};
        result.completion = ScanCompletion::Cancelled;
        result.progress = progress;
        return result;
    });

    CHECK(session.start(L"ignored"));
    session.cancel();
    session.cancel();
    CHECK(wait_for_terminal(session));
    CHECK(session.state() == ScanSessionState::Cancelled);
    CHECK(session.take_result().has_value());
}

TEST_CASE(scan_session_rejects_second_start) {
    ScanSession session([](const auto&, const std::stop_token token, const auto&) {
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ScanResult result{};
        result.completion = ScanCompletion::Cancelled;
        return result;
    });

    CHECK(session.start(L"first"));
    CHECK(!session.start(L"second"));
    session.cancel();
    CHECK(wait_for_terminal(session));
}
