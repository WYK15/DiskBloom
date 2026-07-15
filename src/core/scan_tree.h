#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace diskbloom::core {

using NodeIndex = std::uint32_t;
constexpr NodeIndex invalid_node = std::numeric_limits<NodeIndex>::max();

enum class ScanNodeFlags : std::uint16_t {
    None = 0U,
    Directory = 1U << 0U,
    ReparsePoint = 1U << 1U,
    Inaccessible = 1U << 2U,
    Incomplete = 1U << 3U,
};

[[nodiscard]] constexpr ScanNodeFlags operator|(
    const ScanNodeFlags left,
    const ScanNodeFlags right) noexcept {
    return static_cast<ScanNodeFlags>(
        static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
}

constexpr ScanNodeFlags& operator|=(ScanNodeFlags& left, const ScanNodeFlags right) noexcept {
    left = left | right;
    return left;
}

[[nodiscard]] constexpr bool has_flag(
    const ScanNodeFlags value,
    const ScanNodeFlags flag) noexcept {
    return (static_cast<std::uint16_t>(value) & static_cast<std::uint16_t>(flag)) != 0U;
}

struct ScanNode {
    NodeIndex parent = invalid_node;
    NodeIndex firstChild = invalid_node;
    NodeIndex nextSibling = invalid_node;
    std::uint32_t nameOffset = 0U;
    std::uint32_t nameLength = 0U;
    std::uint64_t logicalSize = 0U;
    std::uint64_t fileCount = 0U;
    std::uint64_t directoryCount = 0U;
    ScanNodeFlags flags = ScanNodeFlags::None;
};

class ScanTree final {
public:
    void reserve(std::size_t nodeCount, std::size_t nameCharacterCount);

    [[nodiscard]] NodeIndex add_root(
        std::wstring_view name,
        ScanNodeFlags flags);
    [[nodiscard]] NodeIndex add_child(
        NodeIndex parent,
        std::wstring_view name,
        std::uint64_t logicalSize,
        ScanNodeFlags flags);
    void add_flags(NodeIndex index, ScanNodeFlags flags) noexcept;

    void aggregate() noexcept;

    [[nodiscard]] const ScanNode& node(NodeIndex index) const noexcept;
    [[nodiscard]] std::wstring_view name(NodeIndex index) const noexcept;
    [[nodiscard]] std::span<const ScanNode> nodes() const noexcept;
    [[nodiscard]] std::size_t estimated_memory_bytes() const noexcept;

private:
    [[nodiscard]] NodeIndex append_node(
        NodeIndex parent,
        std::wstring_view name,
        std::uint64_t logicalSize,
        ScanNodeFlags flags);

    std::vector<ScanNode> nodes_;
    std::vector<wchar_t> names_;
};

} // namespace diskbloom::core
