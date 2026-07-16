#include "app/review_change.h"

#include "app/analyzer_navigation.h"

namespace diskbloom::app {

ReviewMutationResult apply_review_mutation(
    DeletionReview& review,
    const core::ScanTree& tree,
    const ReviewMutation mutation,
    const core::NodeIndex currentRoot,
    const core::NodeIndex selectedNode) {
    ReviewMutationResult result{
        .root = currentRoot,
        .selected = selectedNode,
    };
    result.changed = mutation.kind == ReviewMutationKind::Add
        ? review.add(tree, mutation.node)
        : review.remove(mutation.node);
    result.exclusion.rebuild(tree, review.nodes());
    if (!result.changed) {
        return result;
    }

    AnalyzerNavigationState navigation{
        .view = MainContentView::Analyzer,
        .root = currentRoot,
        .selected = selectedNode,
    };
    (void)reconcile_analyzer_visibility(navigation, tree, result.exclusion);
    result.root = navigation.root;
    result.selected = navigation.selected;
    return result;
}

} // namespace diskbloom::app
