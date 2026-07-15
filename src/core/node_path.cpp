#include "core/node_path.h"

#include <vector>

namespace diskbloom::core {

std::optional<std::wstring> build_node_path(
    const ScanTree& tree,
    const NodeIndex node,
    const std::wstring_view rootPath) {
    if (rootPath.empty() || node >= tree.nodes().size() || tree.nodes().empty()) {
        return std::nullopt;
    }

    std::vector<NodeIndex> ancestors;
    for (auto current = node;
         current != invalid_node && current != 0U;
         current = tree.node(current).parent) {
        if (current >= tree.nodes().size()) {
            return std::nullopt;
        }
        ancestors.push_back(current);
    }
    if (node != 0U && (ancestors.empty()
        || tree.node(ancestors.back()).parent != 0U)) {
        return std::nullopt;
    }

    std::size_t required = rootPath.size();
    for (const auto index : ancestors) {
        required += tree.name(index).size() + 1U;
    }

    std::wstring path;
    path.reserve(required);
    path.append(rootPath);
    for (auto iterator = ancestors.rbegin(); iterator != ancestors.rend(); ++iterator) {
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
            path.push_back(L'\\');
        }
        path.append(tree.name(*iterator));
    }
    return path;
}

} // namespace diskbloom::core
