#include "platform/windows/volume_service.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <string_view>

namespace diskbloom::platform::windows {
namespace {

constexpr std::size_t drive_buffer_size = 512U;
constexpr std::size_t volume_label_size = MAX_PATH + 1U;

std::wstring fallback_display_name(const std::wstring_view root) {
    if (root.size() >= 2U) {
        return std::wstring(root.substr(0U, 2U));
    }
    return std::wstring(root);
}

} // namespace

double used_fraction(const VolumeSnapshot& volume) noexcept {
    if (volume.totalBytes == 0U || volume.freeBytes > volume.totalBytes) {
        return 0.0;
    }

    const auto used = volume.totalBytes - volume.freeBytes;
    return static_cast<double>(used) / static_cast<double>(volume.totalBytes);
}

std::vector<VolumeSnapshot> enumerate_volumes() {
    std::array<wchar_t, drive_buffer_size> drives{};
    const auto length = GetLogicalDriveStringsW(
        static_cast<DWORD>(drives.size()),
        drives.data());
    if (length == 0U || length >= drives.size()) {
        return {};
    }

    std::vector<VolumeSnapshot> volumes;
    volumes.reserve(8U);

    for (const wchar_t* cursor = drives.data(); *cursor != L'\0';) {
        const std::wstring_view root(cursor);
        cursor += root.size() + 1U;

        const auto drive_type = GetDriveTypeW(root.data());
        if (drive_type != DRIVE_FIXED && drive_type != DRIVE_REMOVABLE) {
            continue;
        }

        ULARGE_INTEGER total{};
        ULARGE_INTEGER free{};
        if (!GetDiskFreeSpaceExW(root.data(), nullptr, &total, &free)) {
            continue;
        }

        std::array<wchar_t, volume_label_size> label{};
        GetVolumeInformationW(
            root.data(),
            label.data(),
            static_cast<DWORD>(label.size()),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            0U);

        volumes.push_back({
            std::wstring(root),
            label.front() == L'\0' ? fallback_display_name(root) : std::wstring(label.data()),
            static_cast<std::uint64_t>(total.QuadPart),
            static_cast<std::uint64_t>(free.QuadPart),
            drive_type == DRIVE_FIXED ? VolumeKind::Fixed : VolumeKind::Removable,
        });
    }

    std::sort(volumes.begin(), volumes.end(), [](const auto& left, const auto& right) {
        if (left.kind != right.kind) {
            return left.kind == VolumeKind::Fixed;
        }
        return left.rootPath < right.rootPath;
    });

    return volumes;
}

} // namespace diskbloom::platform::windows
