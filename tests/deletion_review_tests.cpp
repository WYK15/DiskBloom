#include "test_support.h"

#include "app/deletion_review.h"

using diskbloom::app::DeletionReview;
using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;

namespace {

struct ReviewFixture {
    ScanTree tree;
    diskbloom::core::NodeIndex root;
    diskbloom::core::NodeIndex folder;
    diskbloom::core::NodeIndex child;
    diskbloom::core::NodeIndex sibling;

    ReviewFixture()
        : root(tree.add_root(L"root", ScanNodeFlags::Directory)),
          folder(tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory)),
          child(tree.add_child(folder, L"child.bin", 10U, ScanNodeFlags::None)),
          sibling(tree.add_child(folder, L"sibling.bin", 20U, ScanNodeFlags::None)) {
        tree.aggregate();
    }
};

} // namespace

TEST_CASE(deletion_review_keeps_unique_nodes_and_total_bytes) {
    ReviewFixture fixture;
    DeletionReview review;

    CHECK(review.add(fixture.tree, fixture.child));
    CHECK(!review.add(fixture.tree, fixture.child));
    CHECK(review.nodes().size() == 1U);
    CHECK(review.total_bytes() == 10U);
}

TEST_CASE(deletion_review_parent_replaces_selected_descendants) {
    ReviewFixture fixture;
    DeletionReview review;
    CHECK(review.add(fixture.tree, fixture.child));
    CHECK(review.add(fixture.tree, fixture.sibling));

    CHECK(review.add(fixture.tree, fixture.folder));
    CHECK(review.nodes().size() == 1U);
    CHECK(review.nodes()[0] == fixture.folder);
    CHECK(review.total_bytes() == 30U);
    CHECK(!review.add(fixture.tree, fixture.child));
}

TEST_CASE(deletion_review_rejects_scan_root_and_invalid_nodes) {
    ReviewFixture fixture;
    DeletionReview review;
    CHECK(!review.add(fixture.tree, fixture.root));
    CHECK(!review.add(fixture.tree, 99U));
    CHECK(review.nodes().empty());
}

TEST_CASE(deletion_review_removes_nodes_and_clears_total) {
    ReviewFixture fixture;
    DeletionReview review;
    CHECK(review.add(fixture.tree, fixture.child));
    CHECK(review.add(fixture.tree, fixture.sibling));
    CHECK(review.remove(fixture.child));
    CHECK(!review.remove(fixture.child));
    CHECK(review.nodes().size() == 1U);
    CHECK(review.nodes()[0] == fixture.sibling);
    CHECK(review.total_bytes() == 20U);
}
