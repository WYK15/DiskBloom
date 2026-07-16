#include "test_support.h"

#include "app/review_change.h"

using diskbloom::app::DeletionReview;
using diskbloom::app::ReviewMutationKind;
using diskbloom::app::apply_review_mutation;
using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;

namespace {

struct ReviewChangeFixture {
    ScanTree tree;
    diskbloom::core::NodeIndex root;
    diskbloom::core::NodeIndex folder;
    diskbloom::core::NodeIndex nested;
    diskbloom::core::NodeIndex file;
    diskbloom::core::NodeIndex sibling;

    ReviewChangeFixture()
        : root(tree.add_root(L"root", ScanNodeFlags::Directory)),
          folder(tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory)),
          nested(tree.add_child(folder, L"nested", 0U, ScanNodeFlags::Directory)),
          file(tree.add_child(nested, L"file.bin", 25U, ScanNodeFlags::None)),
          sibling(tree.add_child(root, L"sibling.bin", 10U, ScanNodeFlags::None)) {
        tree.aggregate();
    }
};

} // namespace

TEST_CASE(review_change_adds_current_root_and_reconciles_to_parent) {
    ReviewChangeFixture fixture;
    DeletionReview review;

    const auto result = apply_review_mutation(
        review,
        fixture.tree,
        {ReviewMutationKind::Add, fixture.folder},
        fixture.folder,
        fixture.file);

    CHECK(result.changed);
    CHECK(result.root == fixture.root);
    CHECK(result.selected == fixture.root);
    CHECK(result.exclusion.is_excluded_root(fixture.folder));
    CHECK(review.total_bytes() == 25U);
}

TEST_CASE(review_change_restores_exact_item) {
    ReviewChangeFixture fixture;
    DeletionReview review;
    (void)review.add(fixture.tree, fixture.folder);

    const auto result = apply_review_mutation(
        review,
        fixture.tree,
        {ReviewMutationKind::Restore, fixture.folder},
        fixture.root,
        fixture.root);

    CHECK(result.changed);
    CHECK(result.exclusion.empty());
    CHECK(review.nodes().empty());
    CHECK(review.total_bytes() == 0U);
}

TEST_CASE(review_change_rejects_duplicate_absent_and_scan_root_mutations) {
    ReviewChangeFixture fixture;
    DeletionReview review;
    (void)review.add(fixture.tree, fixture.file);
    const auto originalTotal = review.total_bytes();

    CHECK(!apply_review_mutation(
               review,
               fixture.tree,
               {ReviewMutationKind::Add, fixture.file},
               fixture.root,
               fixture.root)
               .changed);
    CHECK(!apply_review_mutation(
               review,
               fixture.tree,
               {ReviewMutationKind::Restore, fixture.sibling},
               fixture.root,
               fixture.root)
               .changed);
    CHECK(!apply_review_mutation(
               review,
               fixture.tree,
               {ReviewMutationKind::Add, fixture.root},
               fixture.root,
               fixture.root)
               .changed);
    CHECK(review.nodes().size() == 1U);
    CHECK(review.nodes()[0] == fixture.file);
    CHECK(review.total_bytes() == originalTotal);
}

TEST_CASE(review_change_parent_replaces_descendant_and_hides_selected_descendant) {
    ReviewChangeFixture fixture;
    DeletionReview review;
    (void)review.add(fixture.tree, fixture.file);

    const auto result = apply_review_mutation(
        review,
        fixture.tree,
        {ReviewMutationKind::Add, fixture.folder},
        fixture.root,
        fixture.file);

    CHECK(result.changed);
    CHECK(review.nodes().size() == 1U);
    CHECK(review.nodes()[0] == fixture.folder);
    CHECK(review.total_bytes() == 25U);
    CHECK(result.root == fixture.root);
    CHECK(result.selected == fixture.root);
}
