#include "test_support.h"

#include "platform/windows/shell_actions.h"

using diskbloom::platform::windows::ShellActionResult;
using diskbloom::platform::windows::open_with_shell;
using diskbloom::platform::windows::reveal_in_explorer;

TEST_CASE(shell_actions_reject_empty_paths) {
    CHECK(open_with_shell({}) == ShellActionResult::InvalidPath);
    CHECK(reveal_in_explorer({}) == ShellActionResult::InvalidPath);
}

TEST_CASE(shell_actions_reject_missing_paths_without_launching_shell) {
    constexpr auto missing = L"Z:\\diskbloom-shell-action-path-that-does-not-exist";
    CHECK(open_with_shell(missing) == ShellActionResult::NotFound);
    CHECK(reveal_in_explorer(missing) == ShellActionResult::NotFound);
}
