#include "app/directory_transition_policy.h"

namespace diskbloom::app {

DirectoryTransitionPolicyChange compute_directory_transition_policy_change(
    const DirectoryTransitionMode mode,
    const bool windowsAnimationsEnabled,
    const bool transitionActive) noexcept {
    const auto animationsEnabled =
        resolve_directory_transitions(mode, windowsAnimationsEnabled);
    return {
        .animationsEnabled = animationsEnabled,
        .completeActiveTransition = transitionActive && !animationsEnabled,
    };
}

} // namespace diskbloom::app
