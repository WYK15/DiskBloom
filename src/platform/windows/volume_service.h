#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace diskbloom::platform::windows {

enum class VolumeKind {
    Fixed,
    Removable,
};

struct VolumeSnapshot {
    std::wstring rootPath;
    std::wstring displayName;
    std::uint64_t totalBytes;
    std::uint64_t freeBytes;
    VolumeKind kind;
};

[[nodiscard]] double used_fraction(const VolumeSnapshot& volume) noexcept;
[[nodiscard]] std::vector<VolumeSnapshot> enumerate_volumes();

} // namespace diskbloom::platform::windows
