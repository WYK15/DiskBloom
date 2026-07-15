#pragma once

#include "core/scan_tree.h"

#include <optional>
#include <string>
#include <string_view>

namespace diskbloom::core {

[[nodiscard]] std::optional<std::wstring> build_node_path(
    const ScanTree& tree,
    NodeIndex node,
    std::wstring_view rootPath);

} // namespace diskbloom::core
