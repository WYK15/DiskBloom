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
    DisksAndFolders,
    DirectoryTransitions,
    AnimationsAlwaysOn,
    AnimationsFollowWindows,
    AnimationsOff,
    CollectorDragHint,
    Count,
};

} // namespace diskbloom::core
