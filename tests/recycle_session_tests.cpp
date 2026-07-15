#include "test_support.h"

#include "app/recycle_session.h"

#include <chrono>
#include <thread>

using diskbloom::app::RecycleSession;
using diskbloom::app::RecycleSessionState;
using diskbloom::platform::windows::RecycleResult;

namespace {

bool wait_for_terminal(RecycleSession& session) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (session.state() != RecycleSessionState::Running) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

} // namespace

TEST_CASE(recycle_session_completes_and_transfers_result_once) {
    RecycleSession session([](const auto& paths, const auto) {
        return paths.size() == 2U
            ? RecycleResult::Succeeded
            : RecycleResult::Failed;
    });

    CHECK(session.state() == RecycleSessionState::Idle);
    CHECK(session.start({L"first", L"second"}, nullptr));
    CHECK(wait_for_terminal(session));
    CHECK(session.state() == RecycleSessionState::Succeeded);
    CHECK(session.take_result() == RecycleResult::Succeeded);
    CHECK(!session.take_result().has_value());
}

TEST_CASE(recycle_session_maps_cancel_and_failure_results) {
    RecycleSession cancelled([](const auto&, const auto) {
        return RecycleResult::Cancelled;
    });
    CHECK(cancelled.start({L"item"}, nullptr));
    CHECK(wait_for_terminal(cancelled));
    CHECK(cancelled.state() == RecycleSessionState::Cancelled);

    RecycleSession failed([](const auto&, const auto) {
        return RecycleResult::Failed;
    });
    CHECK(failed.start({L"item"}, nullptr));
    CHECK(wait_for_terminal(failed));
    CHECK(failed.state() == RecycleSessionState::Failed);
}

TEST_CASE(recycle_session_rejects_empty_and_second_start) {
    RecycleSession session([](const auto&, const auto) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return RecycleResult::Succeeded;
    });

    CHECK(!session.start({}, nullptr));
    CHECK(session.start({L"item"}, nullptr));
    CHECK(!session.start({L"other"}, nullptr));
    CHECK(wait_for_terminal(session));
}
