#pragma once

#include "app/appearance_settings.h"

namespace diskbloom::app {

struct DirectoryTransitionPolicyChange {
    bool animationsEnabled = true;
    bool completeActiveTransition = false;
};

[[nodiscard]] DirectoryTransitionPolicyChange compute_directory_transition_policy_change(
    DirectoryTransitionMode mode,
    bool windowsAnimationsEnabled,
    bool transitionActive) noexcept;

} // namespace diskbloom::app
