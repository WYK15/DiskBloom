#pragma once

#include "app/deletion_review.h"
#include "core/scan_tree_exclusion.h"

namespace diskbloom::app {

enum class ReviewMutationKind {
    Add,
    Restore,
};

struct ReviewMutation {
    ReviewMutationKind kind = ReviewMutationKind::Add;
    core::NodeIndex node = core::invalid_node;
};

struct ReviewMutationResult {
    bool changed = false;
    core::ScanTreeExclusion exclusion;
    core::NodeIndex root = core::invalid_node;
    core::NodeIndex selected = core::invalid_node;
};

[[nodiscard]] ReviewMutationResult apply_review_mutation(
    DeletionReview& review,
    const core::ScanTree& tree,
    ReviewMutation mutation,
    core::NodeIndex currentRoot,
    core::NodeIndex selectedNode);

} // namespace diskbloom::app
