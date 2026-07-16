#include "test_support.h"

#include "platform/windows/system_theme.h"

using diskbloom::platform::windows::resolve_client_area_animations;

TEST_CASE(system_animation_policy_uses_value_on_success) {
    CHECK(resolve_client_area_animations(true, true));
    CHECK(!resolve_client_area_animations(true, false));
}

TEST_CASE(system_animation_policy_defaults_enabled_on_query_failure) {
    CHECK(resolve_client_area_animations(false, false));
    CHECK(resolve_client_area_animations(false, true));
}

