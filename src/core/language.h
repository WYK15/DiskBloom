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
    Scanning,
    Cancelling,
    ScanComplete,
    ScanCancelled,
    ScanFailed,
    Back,
    Overview,
    Items,
    OtherItems,
    Open,
    ShowInExplorer,
    AddToReview,
    DeleteReviewed,
    ConfirmRecycle,
    Recycling,
    ActionFailed,
    Theme,
    ThemeSystem,
    ThemeLight,
    ThemeDark,
    Language,
    English,
    SimplifiedChinese,
    Collected,
    Count,
};

} // namespace diskbloom::core
