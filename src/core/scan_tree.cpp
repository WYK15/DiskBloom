#include "core/scan_tree.h"

namespace diskbloom::core {

void ScanTree::reserve(
    const std::size_t nodeCount,
    const std::size_t nameCharacterCount) {
    nodes_.reserve(nodeCount);
    names_.reserve(nameCharacterCount);
}

NodeIndex ScanTree::add_root(
    const std::wstring_view name,
    const ScanNodeFlags flags) {
    if (!nodes_.empty()) {
        return invalid_node;
    }
    return append_node(invalid_node, name, 0U, flags);
}

NodeIndex ScanTree::add_child(
    const NodeIndex parent,
    const std::wstring_view name,
    const std::uint64_t logicalSize,
    const ScanNodeFlags flags) {
    if (parent >= nodes_.size()) {
        return invalid_node;
    }
    return append_node(parent, name, logicalSize, flags);
}

NodeIndex ScanTree::append_node(
    const NodeIndex parent,
    const std::wstring_view name,
    const std::uint64_t logicalSize,
    const ScanNodeFlags flags) {
    if (nodes_.size() >= static_cast<std::size_t>(invalid_node)
        || name.size() > std::numeric_limits<std::uint32_t>::max()
        || names_.size() > std::numeric_limits<std::uint32_t>::max() - name.size()) {
        return invalid_node;
    }

    const auto index = static_cast<NodeIndex>(nodes_.size());
    const auto name_offset = static_cast<std::uint32_t>(names_.size());
    names_.insert(names_.end(), name.begin(), name.end());

    ScanNode node_value{
        .parent = parent,
        .firstChild = invalid_node,
        .nextSibling = invalid_node,
        .nameOffset = name_offset,
        .nameLength = static_cast<std::uint32_t>(name.size()),
        .logicalSize = logicalSize,
        .fileCount = has_flag(flags, ScanNodeFlags::Directory) ? 0U : 1U,
        .directoryCount = 0U,
        .flags = flags,
    };

    if (parent != invalid_node) {
        node_value.nextSibling = nodes_[parent].firstChild;
        nodes_[parent].firstChild = index;
    }
    nodes_.push_back(node_value);
    return index;
}

void ScanTree::aggregate() noexcept {
    for (auto& value : nodes_) {
        value.fileCount = has_flag(value.flags, ScanNodeFlags::Directory) ? 0U : 1U;
        value.directoryCount = 0U;
        if (has_flag(value.flags, ScanNodeFlags::Directory)) {
            value.logicalSize = 0U;
        }
    }

    for (auto index = nodes_.size(); index > 0U; --index) {
        const auto node_index = index - 1U;
        const auto parent = nodes_[node_index].parent;
        if (parent == invalid_node || parent >= nodes_.size()) {
            continue;
        }

        nodes_[parent].logicalSize += nodes_[node_index].logicalSize;
        nodes_[parent].fileCount += nodes_[node_index].fileCount;
        nodes_[parent].directoryCount += nodes_[node_index].directoryCount;
        if (has_flag(nodes_[node_index].flags, ScanNodeFlags::Directory)) {
            ++nodes_[parent].directoryCount;
        }
    }
}

const ScanNode& ScanTree::node(const NodeIndex index) const noexcept {
    return nodes_[index];
}

std::wstring_view ScanTree::name(const NodeIndex index) const noexcept {
    if (index >= nodes_.size()) {
        return {};
    }
    const auto& value = nodes_[index];
    return {names_.data() + value.nameOffset, value.nameLength};
}

std::span<const ScanNode> ScanTree::nodes() const noexcept {
    return nodes_;
}

std::size_t ScanTree::estimated_memory_bytes() const noexcept {
    return nodes_.capacity() * sizeof(ScanNode) + names_.capacity() * sizeof(wchar_t);
}

} // namespace diskbloom::core
