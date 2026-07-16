#include "test_support.h"

#include "app/directory_transition_policy.h"

using diskbloom::app::DirectoryTransitionMode;
using diskbloom::app::compute_directory_transition_policy_change;

TEST_CASE(turning_directory_transitions_off_stops_active_animation) {
    const auto action = compute_directory_transition_policy_change(
        DirectoryTransitionMode::Off,
        true,
        true);
    CHECK(!action.animationsEnabled);
    CHECK(action.completeActiveTransition);
}

TEST_CASE(system_change_only_stops_follow_system_animation) {
    const auto always = compute_directory_transition_policy_change(
        DirectoryTransitionMode::AlwaysOn,
        false,
        true);
    CHECK(always.animationsEnabled);
    CHECK(!always.completeActiveTransition);

    const auto follow = compute_directory_transition_policy_change(
        DirectoryTransitionMode::FollowSystem,
        false,
        true);
    CHECK(!follow.animationsEnabled);
    CHECK(follow.completeActiveTransition);
}

TEST_CASE(enabled_or_inactive_policy_does_not_request_completion) {
    const auto enabled = compute_directory_transition_policy_change(
        DirectoryTransitionMode::FollowSystem,
        true,
        true);
    CHECK(enabled.animationsEnabled);
    CHECK(!enabled.completeActiveTransition);

    const auto inactive = compute_directory_transition_policy_change(
        DirectoryTransitionMode::Off,
        true,
        false);
    CHECK(!inactive.animationsEnabled);
    CHECK(!inactive.completeActiveTransition);
}
