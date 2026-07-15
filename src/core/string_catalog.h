#pragma once

#include "core/language.h"

#include <string_view>

namespace diskbloom::core {

[[nodiscard]] std::wstring_view get_string(Language language, StringId id) noexcept;

} // namespace diskbloom::core
