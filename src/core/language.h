#pragma once

#include <cstddef>

namespace diskbloom::core {

enum class Language : std::size_t {
    English,
    SimplifiedChinese,
};

enum class StringId : std::size_t {
    AppTitle,
    Scan,
    Cancel,
    ScanFolder,
    Settings,
    UsedOfTotal,
    FixedDrive,
    RemovableDrive,
    ScanFailed,
    Theme,
    ThemeSystem,
    ThemeLight,
    ThemeDark,
    Language,
    English,
    SimplifiedChinese,
    Count,
};

} // namespace diskbloom::core
